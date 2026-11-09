#pragma once

// Обвязка Windows для окна: системные диалоги, открытие файлов, трей, снимок в PNG.

#include "platform.h"

#include <windows.h>
#include <shellapi.h>

#include <cstdint>
#include <string>

namespace sq::gui {

std::string pick_docx(HWND owner);    // пустая строка, если отменено
std::string pick_folder(HWND owner);
void shell_open(const std::string& path);

bool write_png(const fs::path& file, int width, int height, const uint8_t* rgba, int pitch);

class Tray {
public:
    void add(HWND hwnd, HICON icon, UINT callback_message);
    void remove();
    void balloon(const std::string& title, const std::string& text);
    void set_tip(const std::string& tip);

private:
    NOTIFYICONDATAW data_{};
    bool added_ = false;
};

}  // namespace sq::gui
