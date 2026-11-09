#include "events.h"

#include "util.h"

#include <fstream>
#include <sstream>

namespace sq {

namespace {

std::string escape(std::string_view s) {
    std::string r;
    for (char c : s) {
        switch (c) {
            case '\\': r += "\\\\"; break;
            case '\t': r += "\\t"; break;
            case '\n': r += "\\n"; break;
            case '\r': r += "\\r"; break;
            default: r += c;
        }
    }
    return r;
}

std::string unescape(std::string_view s) {
    std::string r;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '\\' || i + 1 == s.size()) {
            r += s[i];
            continue;
        }
        switch (s[++i]) {
            case 't': r += '\t'; break;
            case 'n': r += '\n'; break;
            case 'r': r += '\r'; break;
            default: r += s[i];
        }
    }
    return r;
}

}  // namespace

std::string Event::get(std::string_view name) const {
    for (auto& [k, v] : fields)
        if (k == name) return v;
    return {};
}

Event& Event::set(std::string name, std::string value) {
    for (auto& [k, v] : fields)
        if (k == name) {
            v = std::move(value);
            return *this;
        }
    fields.emplace_back(std::move(name), std::move(value));
    return *this;
}

int EventLog::load() {
    events_.clear();
    keys_.clear();
    std::error_code ec;
    if (!fs::exists(file_, ec)) return 0;
    std::ifstream in(file_, std::ios::binary);
    if (!in) return -1;
    std::stringstream ss;
    ss << in.rdbuf();

    std::string text = ss.str();
    if (starts_with(text, "\xEF\xBB\xBF")) text.erase(0, 3);  // журнал, сохранённый Блокнотом
    int broken = 0;
    for (auto& line : split(text, '\n')) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto parts = split(line, '\t');
        if (parts.size() < 2) {
            ++broken;
            continue;
        }
        Event e;
        e.time = parts[0];
        e.type = parts[1];
        for (size_t i = 2; i < parts.size(); ++i) {
            auto eq = parts[i].find('=');
            if (eq == std::string::npos) continue;
            e.fields.emplace_back(parts[i].substr(0, eq), unescape(std::string_view(parts[i]).substr(eq + 1)));
        }
        if (auto key = e.get("key"); !key.empty()) keys_.insert(key);
        events_.push_back(std::move(e));
    }
    return broken;
}

bool EventLog::append(Event e) {
    if (e.time.empty()) e.time = now_iso();
    std::string line = e.time + "\t" + e.type;
    for (auto& [k, v] : e.fields) line += "\t" + k + "=" + escape(v);
    line += "\n";

    std::error_code ec;
    fs::create_directories(file_.parent_path(), ec);
    std::ofstream out(file_, std::ios::binary | std::ios::app);
    out << line;
    out.flush();
    if (!out) return false;

    if (auto key = e.get("key"); !key.empty()) keys_.insert(key);
    events_.push_back(std::move(e));
    return true;
}

}  // namespace sq
