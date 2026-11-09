#pragma once

// Всё, что зависит от Windows: аргументы и окружение в UTF-8, запуск процессов.

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace sq {

namespace fs = std::filesystem;

std::wstring widen(const std::string& u8);
std::string narrow(const std::wstring& w);

void console_init();
std::vector<std::string> utf8_args();
std::string getenv_u8(const char* name);

fs::path exe_dir();
fs::path to_path(const std::string& u8);
std::string from_path(const fs::path& p);

struct ProcResult {
    int code = -1;
    std::string out;
};

// Запускает exe напрямую через CreateProcessW, без cmd.exe, и собирает stdout.
// exe — полный путь: поиск в текущем каталоге не выполняется.
ProcResult run(const fs::path& exe, const std::vector<std::string>& args);

// Поиск только по PATH, без текущего каталога.
std::optional<fs::path> find_in_path(const std::string& exe_name);

}  // namespace sq
