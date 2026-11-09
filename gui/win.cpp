#include "win.h"

#include <commdlg.h>
#include <shellapi.h>
#include <shobjidl.h>

#include <array>
#include <fstream>
#include <vector>

namespace sq::gui {

std::string pick_docx(HWND owner) {
    wchar_t file[MAX_PATH * 4] = {};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = L"Документ Word (*.docx)\0*.docx\0Все файлы\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = DWORD(std::size(file));
    ofn.lpstrTitle = L"Дизайн-документ механизма";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    return GetOpenFileNameW(&ofn) ? narrow(file) : std::string();
}

std::string pick_folder(HWND owner) {
    std::string result;
    IFileOpenDialog* dialog = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog))))
        return result;
    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    dialog->SetTitle(L"Рабочий репозиторий");
    if (SUCCEEDED(dialog->Show(owner))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dialog->GetResult(&item))) {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                result = narrow(path);
                CoTaskMemFree(path);
            }
            item->Release();
        }
    }
    dialog->Release();
    return result;
}

void shell_open(const std::string& path) {
    ShellExecuteW(nullptr, L"open", widen(path).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

// ---------------------------------------------------------------------------
// PNG без сжатия: deflate «stored»-блоками. Нужен только для снимков окна при проверке.

namespace {

uint32_t crc32(const uint8_t* data, size_t n, uint32_t crc = 0) {
    static const auto table = [] {
        std::array<uint32_t, 256> t{};
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[i] = c;
        }
        return t;
    }();
    crc = ~crc;
    for (size_t i = 0; i < n; ++i) crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

void put32(std::vector<uint8_t>& v, uint32_t x) {
    v.insert(v.end(), {uint8_t(x >> 24), uint8_t(x >> 16), uint8_t(x >> 8), uint8_t(x)});
}

void chunk(std::vector<uint8_t>& out, const char* type, const std::vector<uint8_t>& body) {
    put32(out, uint32_t(body.size()));
    std::vector<uint8_t> typed(type, type + 4);
    typed.insert(typed.end(), body.begin(), body.end());
    out.insert(out.end(), typed.begin(), typed.end());
    put32(out, crc32(typed.data(), typed.size()));
}

}  // namespace

bool write_png(const fs::path& file, int width, int height, const uint8_t* rgba, int pitch) {
    std::vector<uint8_t> raw;
    raw.reserve(size_t(height) * (size_t(width) * 4 + 1));
    for (int y = 0; y < height; ++y) {
        raw.push_back(0);
        const uint8_t* row = rgba + size_t(y) * size_t(pitch);
        for (int x = 0; x < width; ++x) {
            raw.insert(raw.end(), {row[x * 4], row[x * 4 + 1], row[x * 4 + 2], 255});
        }
    }

    std::vector<uint8_t> z = {0x78, 0x01};
    uint32_t a = 1, b = 0;
    for (uint8_t c : raw) {
        a = (a + c) % 65521;
        b = (b + a) % 65521;
    }
    for (size_t pos = 0; pos < raw.size() || pos == 0;) {
        size_t len = std::min<size_t>(65535, raw.size() - pos);
        bool last = pos + len >= raw.size();
        z.push_back(last ? 1 : 0);
        z.insert(z.end(), {uint8_t(len), uint8_t(len >> 8), uint8_t(~len), uint8_t(~len >> 8)});
        z.insert(z.end(), raw.begin() + std::ptrdiff_t(pos), raw.begin() + std::ptrdiff_t(pos + len));
        pos += len;
        if (last) break;
    }
    put32(z, (b << 16) | a);

    std::vector<uint8_t> png = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    std::vector<uint8_t> ihdr;
    put32(ihdr, uint32_t(width));
    put32(ihdr, uint32_t(height));
    ihdr.insert(ihdr.end(), {8, 6, 0, 0, 0});
    chunk(png, "IHDR", ihdr);
    chunk(png, "IDAT", z);
    chunk(png, "IEND", {});

    std::ofstream out(file, std::ios::binary);
    out.write(reinterpret_cast<const char*>(png.data()), std::streamsize(png.size()));
    return bool(out);
}

// ---------------------------------------------------------------------------

void Tray::add(HWND hwnd, HICON icon, UINT callback_message) {
    data_ = {};
    data_.cbSize = sizeof(data_);
    data_.hWnd = hwnd;
    data_.uID = 1;
    data_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    data_.uCallbackMessage = callback_message;
    data_.hIcon = icon;
    wcscpy_s(data_.szTip, L"Security Quest");
    added_ = Shell_NotifyIconW(NIM_ADD, &data_);
    data_.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &data_);
}

void Tray::remove() {
    if (added_) Shell_NotifyIconW(NIM_DELETE, &data_);
    added_ = false;
}

void Tray::balloon(const std::string& title, const std::string& text) {
    if (!added_) return;
    NOTIFYICONDATAW d = data_;
    d.uFlags = NIF_INFO;
    wcsncpy_s(d.szInfoTitle, widen(title).c_str(), _TRUNCATE);
    wcsncpy_s(d.szInfo, widen(text).c_str(), _TRUNCATE);
    d.dwInfoFlags = NIIF_USER | NIIF_LARGE_ICON;
    d.hBalloonIcon = data_.hIcon;
    Shell_NotifyIconW(NIM_MODIFY, &d);
}

void Tray::set_tip(const std::string& tip) {
    if (!added_) return;
    wcsncpy_s(data_.szTip, widen(tip).c_str(), _TRUNCATE);
    data_.uFlags = NIF_TIP | NIF_SHOWTIP;
    Shell_NotifyIconW(NIM_MODIFY, &data_);
    data_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
}

}  // namespace sq::gui
