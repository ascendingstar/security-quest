#include "ini.h"

#include "util.h"

#include <cstdlib>
#include <fstream>
#include <sstream>

namespace sq {

bool Ini::load(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) return false;
    std::stringstream ss;
    ss << in.rdbuf();
    parse(ss.str());
    return true;
}

void Ini::parse(std::string text) {
    if (starts_with(text, "\xEF\xBB\xBF")) text.erase(0, 3);
    order_.clear();
    data_.clear();
    std::string section;
    for (auto& raw : split(text, '\n')) {
        std::string line = trim(raw);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line.front() == '[' && line.back() == ']') {
            section = trim(line.substr(1, line.size() - 2));
            if (!data_.count(section)) order_.push_back(section);
            data_[section];
            continue;
        }
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        data_[section][trim(line.substr(0, eq))] = trim(line.substr(eq + 1));
    }
}

bool Ini::has(const std::string& section) const {
    return data_.count(section) != 0;
}

std::string Ini::get(const std::string& section, const std::string& key, const std::string& def) const {
    auto s = data_.find(section);
    if (s == data_.end()) return def;
    auto k = s->second.find(key);
    return (k == s->second.end() || k->second.empty()) ? def : k->second;
}

double Ini::num(const std::string& section, const std::string& key, double def) const {
    std::string v = get(section, key);
    if (v.empty()) return def;
    char* end = nullptr;
    double d = std::strtod(v.c_str(), &end);
    return (end && *end == '\0') ? d : def;
}

std::vector<std::string> Ini::sections(const std::string& prefix) const {
    std::vector<std::string> out;
    for (auto& s : order_)
        if (starts_with(s, prefix)) out.push_back(s);
    return out;
}

}  // namespace sq
