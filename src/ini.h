#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace sq {

// Минимальный INI: [секция], ключ = значение, комментарии ; и # на отдельной строке.
class Ini {
public:
    bool load(const std::filesystem::path& file);
    void parse(std::string text);

    bool has(const std::string& section) const;
    std::string get(const std::string& section, const std::string& key, const std::string& def = {}) const;
    double num(const std::string& section, const std::string& key, double def) const;

    // Секции, имя которых начинается с prefix, в порядке появления в файле.
    std::vector<std::string> sections(const std::string& prefix) const;

private:
    std::vector<std::string> order_;
    std::map<std::string, std::map<std::string, std::string>> data_;
};

}  // namespace sq
