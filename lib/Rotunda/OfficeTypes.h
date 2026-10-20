#pragma once

#include <cstddef>
#include <cstdint>

// Plain types shared by the Rotunda data module (docs/rotunda/DATA.md).
// No Arduino or HAL dependencies, so host tests compile this directly.
namespace rotunda {

enum class Office : uint8_t { Morning = 0, Noonday, Evening, Compline, Count };
constexpr int OFFICE_COUNT = static_cast<int>(Office::Count);

enum class Rank : uint8_t { Ferial = 0, Sunday, Feast, Fast };

struct Date {
  int16_t year = 0;
  uint8_t month = 0;
  uint8_t day = 0;

  bool valid() const;
  bool operator==(const Date& o) const { return year == o.year && month == o.month && day == o.day; }
  bool operator!=(const Date& o) const { return !(*this == o); }
};

// "YYYY-MM-DD" (only the first 10 characters are read).
bool parseIsoDate(const char* s, Date& out);
// Writes "YYYY-MM-DD"; cap must be at least 11.
void formatIsoDate(const Date& d, char* out, size_t cap);
int daysInMonth(int year, int month);
// Days since 1970-01-01 (proleptic Gregorian).
int32_t dayNumber(const Date& d);
Date fromDayNumber(int32_t days);
Date addDays(const Date& d, int days);
// 0 = Sunday ... 6 = Saturday.
int weekday(const Date& d);

// said.tsv keys: morning, noonday, evening, compline.
const char* officeKey(Office office);
bool parseOfficeKey(const char* key, Office& out);
// EPUB TOC titles, exactly as DATA.md requires.
const char* officeTocTitle(Office office);
bool officeFromTocTitle(const char* title, Office& out);
// Short names used on Home and the sleep screen ("Morning", "Noonday" ...).
const char* officeShortName(Office office);

Rank parseRank(const char* s);

// "HH:MM" -> minutes after midnight; false on malformed input.
bool parseClockMinutes(const char* s, uint16_t& minutes);

// Copies at most cap-1 bytes and always terminates (cap > 0).
void copyField(char* dst, size_t cap, const char* src);

}  // namespace rotunda
