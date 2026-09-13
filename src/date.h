// Small helpers around C++20's <chrono> calendar types.
//
// The two types we use everywhere:
//   Date = std::chrono::sys_days      a specific day, stored as a day count.
//                                     Subtracting two gives a number of days.
//   YMD  = std::chrono::year_month_day  the same day split into y/m/d fields,
//                                     which is what you want for display.
// You can convert freely between them: YMD{someDate} and Date{someYMD}.
#pragma once

#include <chrono>
#include <string>

using Date = std::chrono::sys_days;
using YMD  = std::chrono::year_month_day;

// ---- Construction ---------------------------------------------------------

inline Date makeDate(int y, unsigned m, unsigned d) {
    using namespace std::chrono;
    return Date{year{y} / month{m} / day{d}};
}

inline Date today() {
    using namespace std::chrono;
    return floor<days>(system_clock::now());
}

// ---- Arithmetic -----------------------------------------------------------

// Number of days you must add to `from` to reach `to` (negative if `to` is earlier).
inline int daysBetween(Date from, Date to) {
    return static_cast<int>((to - from).count());
}

inline Date addDays(Date d, int n) {
    return d + std::chrono::days{n};
}

inline Date addMonths(Date d, int n) {
    using namespace std::chrono;
    YMD ymd{d};
    year_month ym = ymd.year() / ymd.month();
    ym += months{n};
    // Clamp to the last valid day, so 31 Jan + 1 month lands on 28/29 Feb
    // instead of producing an invalid 31 Feb.
    YMD moved = ym / ymd.day();
    if (!moved.ok()) {
        moved = ym / last;
    }
    return Date{moved};
}

// ---- Month queries --------------------------------------------------------

inline unsigned daysInMonth(YMD ymd) {
    using namespace std::chrono;
    return unsigned{year_month_day_last{ymd.year(), month_day_last{ymd.month()}}.day()};
}

// 0 = Sunday ... 6 = Saturday
inline int weekdayIndex(Date d) {
    return static_cast<int>(std::chrono::weekday{d}.c_encoding());
}

inline Date firstOfMonth(YMD ymd) {
    using namespace std::chrono;
    return Date{ymd.year() / ymd.month() / day{1}};
}

inline bool sameMonth(YMD a, YMD b) {
    return a.year() == b.year() && a.month() == b.month();
}

// ---- Formatting -----------------------------------------------------------

inline const char* monthName(unsigned m) {
    static const char* kNames[] = {"January", "February", "March",     "April",
                                   "May",     "June",     "July",      "August",
                                   "September", "October", "November", "December"};
    return kNames[m - 1];
}

inline const char* monthAbbrev(unsigned m) {
    static const char* kNames[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    return kNames[m - 1];
}

inline const char* weekdayAbbrev(int idx) {
    static const char* kNames[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    return kNames[idx];
}

// "September 2026"
std::string formatMonthYear(YMD ymd);

// "Sun 13 Sep"
std::string formatShort(Date d);

// "2026-09-13" - the format used in the saved data file.
std::string formatISO(Date d);

// Parses "2026-09-13". Returns false if the line isn't a valid date.
bool parseISO(const std::string& text, Date& out);
