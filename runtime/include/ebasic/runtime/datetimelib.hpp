#pragma once

#include <cstdint>
#include <cstdio>
#include <cmath>
#include <ctime>
#include <chrono>
#include <string>

#include "ebasic/runtime/bstring.hpp"

/// FreeBASIC-style date/time procedures - eBasic's own standard library,
/// made available in every compiled program (never opt-in) via the same
/// compiler-injected `Extern "C++"` prelude the string/file/math
/// libraries already use (see
/// compiler/src/preprocessor/builtin_prelude.hpp). Pulled in
/// unconditionally by runtime.hpp - no linking step needed.
///
/// A date+time is a plain DOUBLE "serial number" - whole days since
/// 1899-12-30 (matching real FreeBASIC's own internal convention
/// exactly), with the fractional part representing time-of-day. This is
/// what makes DateAdd/arithmetic on the result trivial for day-and-
/// smaller units (`serial + 1.0` is "the same time, one day later").
///
/// Calendar math uses Howard Hinnant's well-known, exact
/// days_from_civil/civil_from_days integer algorithms for the proleptic
/// Gregorian calendar (http://howardhinnant.github.io/date_algorithms.html)
/// rather than `<chrono>`'s C++20 calendar types, since this project's
/// baseline is C++17.
namespace ebasic::rt::datetimelib {

namespace detail {

/// Days since 1970-01-01 (negative before it) for a proleptic Gregorian
/// y/m/d - Howard Hinnant's days_from_civil.
inline std::int64_t daysFromCivil(std::int64_t y, unsigned m, unsigned d) {
    y -= (m <= 2) ? 1 : 0;
    const std::int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<std::int64_t>(doe) - 719468;
}

struct Civil {
    std::int64_t year;
    unsigned month;
    unsigned day;
};

/// The inverse of daysFromCivil - Howard Hinnant's civil_from_days.
inline Civil civilFromDays(std::int64_t z) {
    z += 719468;
    const std::int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const std::int64_t y = static_cast<std::int64_t>(yoe) + era * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    const unsigned d = doy - (153 * mp + 2) / 5 + 1;
    const unsigned m = mp + (mp < 10 ? 3 : -9);
    return Civil{y + (m <= 2 ? 1 : 0), m, d};
}

/// Days-since-1970-01-01 for this library's own epoch, 1899-12-30 -
/// real FreeBASIC's own date-serial epoch.
inline std::int64_t epochDays() { return daysFromCivil(1899, 12, 30); }

inline bool isLeapYear(std::int64_t y) {
    return (y % 4 == 0) && (y % 100 != 0 || y % 400 == 0);
}

inline unsigned daysInMonth(std::int64_t y, unsigned m) {
    static const unsigned kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m == 2 && isLeapYear(y)) return 29;
    return kDays[m - 1];
}

/// The whole-day part of a serial, floored (not truncated) so negative
/// serials (dates before the epoch) still split cleanly into a whole
/// day plus a [0, 1) fractional time-of-day.
inline std::int64_t wholeDays(double serial) {
    return static_cast<std::int64_t>(std::floor(serial));
}

inline double fractionalDay(double serial) { return serial - std::floor(serial); }

/// Rounds a fractional day to the nearest whole second, to avoid
/// floating-point noise splitting an exact time (e.g. noon) across a
/// second boundary.
inline std::int64_t secondsOfDay(double serial) {
    std::int64_t s = static_cast<std::int64_t>(std::llround(fractionalDay(serial) * 86400.0));
    if (s >= 86400) s = 86399; // clamp a rounding-induced overflow into the next day
    return s;
}

} // namespace detail

inline double DateSerial(std::int32_t year, std::int32_t month, std::int32_t day) {
    return static_cast<double>(detail::daysFromCivil(year, static_cast<unsigned>(month), static_cast<unsigned>(day)) - detail::epochDays());
}

inline double TimeSerial(std::int32_t hour, std::int32_t minute, std::int32_t second) {
    return (hour * 3600.0 + minute * 60.0 + second) / 86400.0;
}

inline std::int32_t Year(double serial) {
    return static_cast<std::int32_t>(detail::civilFromDays(detail::wholeDays(serial) + detail::epochDays()).year);
}

inline std::int32_t Month(double serial) {
    return static_cast<std::int32_t>(detail::civilFromDays(detail::wholeDays(serial) + detail::epochDays()).month);
}

inline std::int32_t Day(double serial) {
    return static_cast<std::int32_t>(detail::civilFromDays(detail::wholeDays(serial) + detail::epochDays()).day);
}

inline std::int32_t Hour(double serial) {
    return static_cast<std::int32_t>(detail::secondsOfDay(serial) / 3600);
}

inline std::int32_t Minute(double serial) {
    return static_cast<std::int32_t>((detail::secondsOfDay(serial) % 3600) / 60);
}

inline std::int32_t Second(double serial) {
    return static_cast<std::int32_t>(detail::secondsOfDay(serial) % 60);
}

/// 1=Sunday .. 7=Saturday, matching real FreeBASIC's own Weekday.
inline std::int32_t Weekday(double serial) {
    std::int64_t z = detail::wholeDays(serial) + detail::epochDays(); // days since 1970-01-01
    // 1970-01-01 was a Thursday; floor-mod keeps this correct for
    // negative z (dates before 1970) too.
    std::int64_t w = ((z % 7) + 7 + 4) % 7; // 0 = Sunday .. 6 = Saturday
    return static_cast<std::int32_t>(w + 1);
}

/// Adds `number` of the unit named by `intervalCode` ("yyyy"/"m"/"d"/
/// "h"/"n"/"s", matching real FreeBASIC's own DateAdd codes) to `serial`.
/// Calendar units ("yyyy"/"m") clamp the day-of-month if the target
/// month is shorter (e.g. adding 1 month to Jan 31 lands on Feb 28/29,
/// not March 3) and preserve the original time-of-day exactly; the
/// fixed-length units ("d"/"h"/"n"/"s") are plain serial arithmetic.
inline double DateAdd(BString intervalCode, double number, double serial) {
    std::string code = intervalCode.str();
    double frac = detail::fractionalDay(serial);
    std::int64_t days = detail::wholeDays(serial);

    if (code == "yyyy" || code == "m") {
        detail::Civil c = detail::civilFromDays(days + detail::epochDays());
        std::int64_t totalMonths = c.year * 12 + (c.month - 1);
        if (code == "yyyy") {
            totalMonths += static_cast<std::int64_t>(number) * 12;
        } else {
            totalMonths += static_cast<std::int64_t>(number);
        }
        std::int64_t newYear = totalMonths / 12;
        unsigned newMonth = static_cast<unsigned>(totalMonths % 12);
        if (totalMonths < 0 && newMonth != 0) {
            // C++'s truncating integer division/modulo needs a floor
            // correction for a negative total-months count.
            newYear -= 1;
            newMonth += 12;
        }
        newMonth += 1; // back to 1-based
        unsigned newDay = c.day;
        unsigned maxDay = detail::daysInMonth(newYear, newMonth);
        if (newDay > maxDay) newDay = maxDay;
        std::int64_t newDays = detail::daysFromCivil(newYear, newMonth, newDay) - detail::epochDays();
        return static_cast<double>(newDays) + frac;
    }
    if (code == "d") return serial + number;
    if (code == "h") return serial + number / 24.0;
    if (code == "n") return serial + number / 1440.0;
    if (code == "s") return serial + number / 86400.0;
    return serial;
}

/// The number of calendar-unit boundaries crossed between `startSerial`
/// and `endSerial` for "yyyy"/"m" (matching real FreeBASIC/Visual
/// Basic's own DateDiff semantics - e.g. DateDiff("yyyy", Dec 31, Jan 1)
/// is 1, even though only one day elapsed), or the exact elapsed
/// duration in that unit for "d"/"h"/"n"/"s".
inline double DateDiff(BString intervalCode, double startSerial, double endSerial) {
    std::string code = intervalCode.str();
    if (code == "yyyy" || code == "m") {
        detail::Civil s = detail::civilFromDays(detail::wholeDays(startSerial) + detail::epochDays());
        detail::Civil e = detail::civilFromDays(detail::wholeDays(endSerial) + detail::epochDays());
        if (code == "yyyy") return static_cast<double>(e.year - s.year);
        return static_cast<double>((e.year * 12 + e.month) - (s.year * 12 + s.month));
    }
    double diff = endSerial - startSerial;
    if (code == "d") return diff;
    if (code == "h") return diff * 24.0;
    if (code == "n") return diff * 1440.0;
    if (code == "s") return diff * 86400.0;
    return diff;
}

/// The current local date+time as a serial.
inline double Now() {
    std::time_t t = std::time(nullptr);
    std::tm local = *std::localtime(&t);
    std::int64_t days = detail::daysFromCivil(local.tm_year + 1900, static_cast<unsigned>(local.tm_mon + 1), static_cast<unsigned>(local.tm_mday)) - detail::epochDays();
    double frac = (local.tm_hour * 3600.0 + local.tm_min * 60.0 + local.tm_sec) / 86400.0;
    return static_cast<double>(days) + frac;
}

/// Seconds since midnight, local time.
inline double Timer() {
    std::time_t t = std::time(nullptr);
    std::tm local = *std::localtime(&t);
    return local.tm_hour * 3600.0 + local.tm_min * 60.0 + local.tm_sec;
}

/// The current local date, formatted as "YYYY-MM-DD". A deliberate
/// ISO-8601-style choice, not a reproduction of real FreeBASIC's own
/// locale-dependent legacy "mm-dd-yyyy" Date$ format.
inline BString Date() {
    std::time_t t = std::time(nullptr);
    std::tm local = *std::localtime(&t);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);
    return BString(std::string(buf));
}

/// The current local time, formatted as "HH:MM:SS" (24-hour).
inline BString Time() {
    std::time_t t = std::time(nullptr);
    std::tm local = *std::localtime(&t);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", local.tm_hour, local.tm_min, local.tm_sec);
    return BString(std::string(buf));
}

} // namespace ebasic::rt::datetimelib

// Thin global forwarders - the compiler's own builtin prelude declares
// these exact bare names via `Extern "C++"` with no `Namespace` block
// (so they're callable unqualified from any .bas program), which means
// the real, linkable C++ symbol must be a plain top-level function of
// that same name - the actual logic stays namespaced above.
inline double DateSerial(std::int32_t year, std::int32_t month, std::int32_t day) {
    return ::ebasic::rt::datetimelib::DateSerial(year, month, day);
}
inline double TimeSerial(std::int32_t hour, std::int32_t minute, std::int32_t second) {
    return ::ebasic::rt::datetimelib::TimeSerial(hour, minute, second);
}
inline std::int32_t Year(double serial) { return ::ebasic::rt::datetimelib::Year(serial); }
inline std::int32_t Month(double serial) { return ::ebasic::rt::datetimelib::Month(serial); }
inline std::int32_t Day(double serial) { return ::ebasic::rt::datetimelib::Day(serial); }
inline std::int32_t Hour(double serial) { return ::ebasic::rt::datetimelib::Hour(serial); }
inline std::int32_t Minute(double serial) { return ::ebasic::rt::datetimelib::Minute(serial); }
inline std::int32_t Second(double serial) { return ::ebasic::rt::datetimelib::Second(serial); }
inline std::int32_t Weekday(double serial) { return ::ebasic::rt::datetimelib::Weekday(serial); }
inline double DateAdd(::ebasic::rt::BString intervalCode, double number, double serial) {
    return ::ebasic::rt::datetimelib::DateAdd(std::move(intervalCode), number, serial);
}
inline double DateDiff(::ebasic::rt::BString intervalCode, double startSerial, double endSerial) {
    return ::ebasic::rt::datetimelib::DateDiff(std::move(intervalCode), startSerial, endSerial);
}
inline double Now() { return ::ebasic::rt::datetimelib::Now(); }
inline double Timer() { return ::ebasic::rt::datetimelib::Timer(); }
inline ::ebasic::rt::BString Date() { return ::ebasic::rt::datetimelib::Date(); }
inline ::ebasic::rt::BString Time() { return ::ebasic::rt::datetimelib::Time(); }
