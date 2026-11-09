#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace sq {

std::string trim(std::string_view s);
std::vector<std::string> split(std::string_view s, char sep);
std::string upper_ascii(std::string_view s);
bool starts_with(std::string_view s, std::string_view prefix);

struct Date {
    int y = 0, m = 0, d = 0;
};

std::string now_iso();                    // местное время, YYYY-MM-DDTHH:MM:SS
Date today();
Date parse_date(std::string_view iso);    // берутся первые 10 символов
std::string format_date(const Date& d);   // YYYY-MM-DD
std::string week_start(const Date& d);    // понедельник недели, YYYY-MM-DD
Date add_months(const Date& d, int months);
Date add_days(const Date& d, int days);
std::string month_key(const Date& d);     // YYYY-MM

std::string month_nominative(int m);      // «ноябрь»
std::string month_genitive(int m);        // «ноября»

std::string progress_bar(double fraction, int width);

}  // namespace sq
