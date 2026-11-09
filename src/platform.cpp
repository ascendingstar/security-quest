#include "platform.h"

#include "util.h"

#include <windows.h>
#include <shellapi.h>

namespace sq {

std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0);
    std::wstring w(size_t(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), w.data(), n);
    return w;
}

std::string narrow(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(size_t(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), s.data(), n, nullptr, nullptr);
    return s;
}

namespace {

// Правила разбора командной строки MSVCRT: обратные слэши перед кавычкой удваиваются.
void append_quoted(std::wstring& cmd, const std::wstring& arg) {
    if (!arg.empty() && arg.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
        cmd += arg;
        return;
    }
    cmd += L'"';
    for (auto it = arg.begin();; ++it) {
        size_t backslashes = 0;
        while (it != arg.end() && *it == L'\\') {
            ++it;
            ++backslashes;
        }
        if (it == arg.end()) {
            cmd.append(backslashes * 2, L'\\');
            break;
        }
        if (*it == L'"') {
            cmd.append(backslashes * 2 + 1, L'\\');
            cmd += L'"';
        } else {
            cmd.append(backslashes, L'\\');
            cmd += *it;
        }
    }
    cmd += L'"';
}

}  // namespace

void console_init() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
}

std::vector<std::string> utf8_args() {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::vector<std::string> args;
    for (int i = 0; i < argc; ++i) args.push_back(narrow(argv[i]));
    LocalFree(argv);
    return args;
}

std::string getenv_u8(const char* name) {
    std::wstring wname = widen(name);
    DWORD n = GetEnvironmentVariableW(wname.c_str(), nullptr, 0);
    if (n == 0) return {};
    std::wstring value(n, L'\0');
    n = GetEnvironmentVariableW(wname.c_str(), value.data(), n);
    value.resize(n);
    return narrow(value);
}

fs::path exe_dir() {
    std::wstring buf(MAX_PATH, L'\0');
    while (true) {
        DWORD n = GetModuleFileNameW(nullptr, buf.data(), DWORD(buf.size()));
        if (n == 0) return {};
        if (n < buf.size()) {
            buf.resize(n);
            return fs::path(buf).parent_path();
        }
        buf.resize(buf.size() * 2);
    }
}

fs::path to_path(const std::string& u8) {
    return fs::path(std::u8string(reinterpret_cast<const char8_t*>(u8.data()), u8.size()));
}

std::string from_path(const fs::path& p) {
    auto u = p.u8string();
    return std::string(u.begin(), u.end());
}

ProcResult run(const fs::path& exe, const std::vector<std::string>& args) {
    ProcResult result;

    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE read_end = nullptr, write_end = nullptr;
    if (!CreatePipe(&read_end, &write_end, &sa, 0)) return result;
    SetHandleInformation(read_end, HANDLE_FLAG_INHERIT, 0);

    std::wstring cmd;
    append_quoted(cmd, exe.wstring());
    for (const auto& a : args) {
        cmd += L' ';
        append_quoted(cmd, widen(a));
    }

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = write_end;
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    // Из оконного приложения консольный git иначе мелькал бы чёрным окном.
    DWORD flags = GetConsoleWindow() ? 0 : CREATE_NO_WINDOW;
    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, TRUE, flags, nullptr, nullptr, &si, &pi);
    CloseHandle(write_end);
    if (!ok) {
        CloseHandle(read_end);
        return result;
    }

    char buf[65536];
    DWORD n = 0;
    while (ReadFile(read_end, buf, sizeof buf, &n, nullptr) && n > 0) result.out.append(buf, n);
    CloseHandle(read_end);

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    result.code = int(code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return result;
}

std::optional<fs::path> find_in_path(const std::string& exe_name) {
    for (auto& dir : split(getenv_u8("PATH"), ';')) {
        std::string d = trim(dir);
        if (d.size() >= 2 && d.front() == '"' && d.back() == '"') d = d.substr(1, d.size() - 2);
        if (d.empty()) continue;
        fs::path candidate = to_path(d) / to_path(exe_name);
        std::error_code ec;
        if (candidate.is_absolute() && fs::is_regular_file(candidate, ec)) return candidate;
    }
    return std::nullopt;
}

}  // namespace sq
