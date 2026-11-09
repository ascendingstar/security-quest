#pragma once

// Журнал событий: единственный источник истины. Состояние игры и очки
// каждый раз пересчитываются из него (см. game.h).
//
// Формат строки: время \t тип \t ключ=значение \t ...
// В значениях экранируются \\ \t \n \r. Поле key, если есть, делает событие
// уникальным: синхронизация не пишет одно и то же наблюдение дважды.

#include "platform.h"

#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sq {

struct Event {
    std::string time;
    std::string type;
    std::vector<std::pair<std::string, std::string>> fields;

    std::string get(std::string_view name) const;
    Event& set(std::string name, std::string value);
};

class EventLog {
public:
    explicit EventLog(fs::path file) : file_(std::move(file)) {}

    // Возвращает число пропущенных повреждённых строк, или -1, если файл не прочитан.
    int load();
    bool append(Event e);
    bool has_key(const std::string& key) const { return keys_.count(key) != 0; }
    const std::vector<Event>& all() const { return events_; }

private:
    fs::path file_;
    std::vector<Event> events_;
    std::set<std::string> keys_;
};

}  // namespace sq
