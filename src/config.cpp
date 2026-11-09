#include "config.h"

#include "util.h"
#include "version.h"

#include <algorithm>
#include <format>
#include <fstream>
#include <functional>
#include <sstream>

namespace sq {

fs::path data_home() {
    std::string explicit_home = getenv_u8("SQ_HOME");
    if (!explicit_home.empty()) return to_path(explicit_home);
    // Портативный режим: папка data рядом с exe, например на флешке.
    std::error_code ec;
    fs::path portable = exe_dir() / "data";
    if (fs::is_directory(portable, ec)) return portable;
    return to_path(getenv_u8("LOCALAPPDATA")) / "SecurityQuest";
}

const char* const kDefaultConfig = R"(; Security Quest — настройки. Файл в UTF-8.
; Награды пересчитываются из журнала при каждом запуске,
; поэтому таблицу в [rewards] можно менять задним числом.

[rewards]
; Миссии: значения для сложности 1, 2, 3, 4, 5.
mission_xp = 10, 20, 35, 55, 80
mission_gold = 30, 60, 100, 160, 240
; Шанс трофея в процентах для миссий сложности 4 и 5.
artifact_chance_4 = 35
artifact_chance_5 = 100
; Побочные квесты: лёгкий, средний, сложный.
side_xp = 5, 10, 20
side_gold = 20, 45, 90

[gui]
; Закрытие окна сворачивает в трей (1) или закрывает программу (0).
close_to_tray = 1
)";

namespace {

bool read_file(const fs::path& file, std::string& out) {
    std::ifstream in(file, std::ios::binary);
    if (!in) return false;
    std::stringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

}  // namespace

bool set_ini_value(const fs::path& file, const std::string& section, const std::string& key,
                   const std::string& value) {
    std::string text;
    if (!read_file(file, text)) return false;
    auto lines = split(text, '\n');
    std::string entry = key + " = " + value;

    auto save = [&] {
        std::string joined;
        for (size_t j = 0; j < lines.size(); ++j) joined += lines[j] + (j + 1 < lines.size() ? "\n" : "");
        std::ofstream out(file, std::ios::binary | std::ios::trunc);
        out << joined;
        return bool(out);
    };

    bool in_section = false;
    size_t header = std::string::npos;
    for (size_t i = 0; i < lines.size(); ++i) {
        std::string line = trim(lines[i]);
        if (!line.empty() && line.front() == '[' && line.back() == ']') {
            if (in_section) break;
            in_section = trim(line.substr(1, line.size() - 2)) == section;
            if (in_section) header = i;
            continue;
        }
        if (!in_section || line.empty() || line[0] == ';' || line[0] == '#') continue;
        auto eq = line.find('=');
        if (eq != std::string::npos && trim(line.substr(0, eq)) == key) {
            lines[i] = entry;
            return save();
        }
    }

    if (header != std::string::npos) {
        lines.insert(lines.begin() + std::ptrdiff_t(header) + 1, entry);
        return save();
    }
    return append_text(file, "\n[" + section + "]\n" + entry + "\n");
}

std::string check_data_format(Config& cfg) {
    int format = int(cfg.ini.num("data", "format", 0));
    if (format > kDataFormat)
        return std::format("Данные созданы более новой версией программы (формат {}, эта версия понимает {}). "
                           "Обновите программу: иначе часть прогресса может отображаться неверно.",
                           format, kDataFormat);
    if (format < kDataFormat) {
        // Данные версий до 0.4 совместимы как есть: формат просто проставляется.
        set_ini_value(cfg.config_file(), "data", "format", std::to_string(kDataFormat));
        cfg.ini.load(cfg.config_file());
    }
    return {};
}

std::vector<fs::path> list_backups(const Config& cfg) {
    std::vector<fs::path> out;
    std::error_code ec;
    for (auto& entry : fs::directory_iterator(cfg.home / "backups", ec))
        if (entry.is_directory()) out.push_back(entry.path());
    std::ranges::sort(out, std::greater<>());
    return out;
}

bool make_backup(const Config& cfg, const std::string& label, fs::path* folder) {
    std::string stamp = now_iso();
    std::ranges::replace(stamp, 'T', ' ');
    std::ranges::replace(stamp, ':', '-');
    fs::path dir = cfg.home / "backups" / to_path(stamp + (label.empty() ? "" : " " + label));
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec) return false;
    bool any = false;
    for (const char* name : {"config.ini", "events.log", "game.log"}) {
        fs::path src = cfg.home / name;
        if (!fs::exists(src, ec)) continue;
        fs::copy_file(src, dir / name, fs::copy_options::overwrite_existing, ec);
        if (ec) return false;
        any = true;
    }
    if (folder) *folder = dir;
    auto all = list_backups(cfg);
    for (size_t i = 30; i < all.size(); ++i) fs::remove_all(all[i], ec);
    return any;
}

bool append_text(const fs::path& file, const std::string& text) {
    std::ofstream out(file, std::ios::binary | std::ios::app);
    out << text;
    return bool(out);
}

}  // namespace sq
