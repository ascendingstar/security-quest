#include "util.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>

namespace sq {

std::string trim(std::string_view s) {
    const char* ws = " \t\r\n";
    auto b = s.find_first_not_of(ws);
    if (b == std::string_view::npos) return {};
    auto e = s.find_last_not_of(ws);
    return std::string(s.substr(b, e - b + 1));
}

std::vector<std::string> split(std::string_view s, char sep) {
    std::vector<std::string> out;
    size_t start = 0;
    while (true) {
        auto pos = s.find(sep, start);
        out.emplace_back(s.substr(start, pos == std::string_view::npos ? std::string_view::npos : pos - start));
        if (pos == std::string_view::npos) break;
        start = pos + 1;
    }
    return out;
}

std::string upper_ascii(std::string_view s) {
    std::string r(s);
    std::ranges::transform(r, r.begin(), [](char c) { return (c >= 'a' && c <= 'z') ? char(c - 'a' + 'A') : c; });
    return r;
}

bool starts_with(std::string_view s, std::string_view prefix) {
    return s.substr(0, prefix.size()) == prefix;
}

std::string now_iso() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &t);
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%S", &tm);
    return buf;
}

Date today() {
    return parse_date(now_iso());
}

Date parse_date(std::string_view iso) {
    Date d;
    if (iso.size() >= 10) std::sscanf(std::string(iso.substr(0, 10)).c_str(), "%d-%d-%d", &d.y, &d.m, &d.d);
    return d;
}

std::string format_date(const Date& d) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%04d-%02d-%02d", d.y, d.m, d.d);
    return buf;
}

std::string week_start(const Date& d) {
    using namespace std::chrono;
    sys_days day = year_month_day{year{d.y}, month{unsigned(d.m)}, std::chrono::day{unsigned(d.d)}};
    sys_days monday = day - days{weekday{day}.iso_encoding() - 1};
    year_month_day ymd{monday};
    return format_date({int(ymd.year()), int(unsigned(ymd.month())), int(unsigned(ymd.day()))});
}

Date add_months(const Date& d, int months) {
    int total = d.y * 12 + (d.m - 1) + months;
    return {total / 12, total % 12 + 1, d.d};
}

Date add_days(const Date& d, int n) {
    using namespace std::chrono;
    if (d.y == 0) return d;
    sys_days day = year_month_day{year{d.y}, month{unsigned(d.m)}, std::chrono::day{unsigned(d.d)}};
    year_month_day ymd{day + days{n}};
    return {int(ymd.year()), int(unsigned(ymd.month())), int(unsigned(ymd.day()))};
}

std::string month_key(const Date& d) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%04d-%02d", d.y, d.m);
    return buf;
}

std::string month_nominative(int m) {
    static const char* names[] = {"январь", "февраль", "март",     "апрель",  "май",    "июнь",
                                  "июль",   "август",  "сентябрь", "октябрь", "ноябрь", "декабрь"};
    return (m >= 1 && m <= 12) ? names[m - 1] : "?";
}

std::string month_genitive(int m) {
    static const char* names[] = {"января", "февраля", "марта",    "апреля",  "мая",    "июня",
                                  "июля",   "августа", "сентября", "октября", "ноября", "декабря"};
    return (m >= 1 && m <= 12) ? names[m - 1] : "?";
}

std::string progress_bar(double fraction, int width) {
    int filled = std::clamp(int(fraction * width + 0.5), 0, width);
    std::string r;
    for (int i = 0; i < width; ++i) r += i < filled ? "█" : "░";
    return r;
}

}  // namespace sq
