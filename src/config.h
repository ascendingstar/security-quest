#pragma once

#include "ini.h"
#include "platform.h"

#include <string>
#include <vector>

namespace sq {

struct Config {
    fs::path home;  // каталог данных: config.ini и events.log
    Ini ini;

    fs::path config_file() const { return home / "config.ini"; }
    fs::path events_file() const { return home / "events.log"; }
    fs::path game_file() const { return home / "game.log"; }
};

// SQ_HOME, иначе папка data рядом с exe, иначе %LOCALAPPDATA%\SecurityQuest.
fs::path data_home();

extern const char* const kDefaultConfig;

// Меняет или добавляет «key = value» в секции, сохраняя остальной файл и комментарии.
bool set_ini_value(const fs::path& file, const std::string& section, const std::string& key,
                   const std::string& value);
bool append_text(const fs::path& file, const std::string& text);

// Сверяет [data] format в config.ini с форматом программы и проставляет его у новых данных.
// Возвращает предупреждение, если данные созданы более новой версией программы.
std::string check_data_format(Config& cfg);

// Резервная копия config.ini, events.log и game.log в data\backups\<дата время>[ метка].
// Хранится 30 последних. folder — куда сохранено.
bool make_backup(const Config& cfg, const std::string& label, fs::path* folder = nullptr);
std::vector<fs::path> list_backups(const Config& cfg);  // от новых к старым

}  // namespace sq
