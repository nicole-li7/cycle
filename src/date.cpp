#include "date.h"

#include <cstdio>

std::string formatMonthYear(YMD ymd) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%s %d",
                  monthName(unsigned{ymd.month()}), int{ymd.year()});
    return buf;
}

std::string formatShort(Date d) {
    YMD ymd{d};
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%s %u %s",
                  weekdayAbbrev(weekdayIndex(d)),
                  unsigned{ymd.day()},
                  monthAbbrev(unsigned{ymd.month()}));
    return buf;
}

std::string formatISO(Date d) {
    YMD ymd{d};
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%04d-%02u-%02u",
                  int{ymd.year()}, unsigned{ymd.month()}, unsigned{ymd.day()});
    return buf;
}

bool parseISO(const std::string& text, Date& out) {
    int y = 0;
    unsigned m = 0, d = 0;
    if (std::sscanf(text.c_str(), "%d-%u-%u", &y, &m, &d) != 3) {
        return false;
    }
    YMD ymd{std::chrono::year{y}, std::chrono::month{m}, std::chrono::day{d}};
    if (!ymd.ok()) {
        return false;
    }
    out = Date{ymd};
    return true;
}
