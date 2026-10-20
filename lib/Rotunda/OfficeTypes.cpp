#include "OfficeTypes.h"

#include <cstdio>
#include <cstring>

namespace rotunda {

namespace {
constexpr const char* OFFICE_KEYS[OFFICE_COUNT] = {"morning", "noonday", "evening", "compline"};
constexpr const char* OFFICE_TOC_TITLES[OFFICE_COUNT] = {"Morning Prayer", "Noonday Prayer", "Evening Prayer",
                                                         "Compline"};
constexpr const char* OFFICE_SHORT_NAMES[OFFICE_COUNT] = {"Morning", "Noonday", "Evening", "Compline"};

bool isDigit(const char c) { return c >= '0' && c <= '9'; }

bool parseDigits(const char* s, const int count, int& out) {
  out = 0;
  for (int i = 0; i < count; ++i) {
    if (!isDigit(s[i])) return false;
    out = out * 10 + (s[i] - '0');
  }
  return true;
}
}  // namespace

bool Date::valid() const {
  return year > 0 && month >= 1 && month <= 12 && day >= 1 && day <= daysInMonth(year, month);
}

bool parseIsoDate(const char* s, Date& out) {
  if (!s) return false;
  int y = 0;
  int m = 0;
  int d = 0;
  if (!parseDigits(s, 4, y) || s[4] != '-' || !parseDigits(s + 5, 2, m) || s[7] != '-' || !parseDigits(s + 8, 2, d)) {
    return false;
  }
  Date parsed;
  parsed.year = static_cast<int16_t>(y);
  parsed.month = static_cast<uint8_t>(m);
  parsed.day = static_cast<uint8_t>(d);
  if (!parsed.valid()) return false;
  out = parsed;
  return true;
}

void formatIsoDate(const Date& d, char* out, const size_t cap) {
  snprintf(out, cap, "%04d-%02d-%02d", d.year, d.month, d.day);
}

int daysInMonth(const int year, const int month) {
  static constexpr uint8_t DAYS[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month < 1 || month > 12) return 0;
  if (month == 2) {
    const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    return leap ? 29 : 28;
  }
  return DAYS[month - 1];
}

// Howard Hinnant's days_from_civil / civil_from_days.
int32_t dayNumber(const Date& d) {
  const int y = d.year - (d.month <= 2 ? 1 : 0);
  const int era = (y >= 0 ? y : y - 399) / 400;
  const int yoe = y - era * 400;
  const int mp = d.month > 2 ? d.month - 3 : d.month + 9;
  const int doy = (153 * mp + 2) / 5 + d.day - 1;
  const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

Date fromDayNumber(int32_t days) {
  days += 719468;
  const int32_t era = (days >= 0 ? days : days - 146096) / 146097;
  const int32_t doe = days - era * 146097;
  const int32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const int32_t mp = (5 * doy + 2) / 153;
  const int32_t d = doy - (153 * mp + 2) / 5 + 1;
  const int32_t m = mp < 10 ? mp + 3 : mp - 9;
  Date out;
  out.year = static_cast<int16_t>(yoe + era * 400 + (m <= 2 ? 1 : 0));
  out.month = static_cast<uint8_t>(m);
  out.day = static_cast<uint8_t>(d);
  return out;
}

Date addDays(const Date& d, const int days) { return fromDayNumber(dayNumber(d) + days); }

int weekday(const Date& d) {
  // 1970-01-01 was a Thursday (4).
  const int32_t n = dayNumber(d);
  const int32_t w = (n + 4) % 7;
  return static_cast<int>(w < 0 ? w + 7 : w);
}

const char* officeKey(const Office office) {
  const int i = static_cast<int>(office);
  return i >= 0 && i < OFFICE_COUNT ? OFFICE_KEYS[i] : "";
}

bool parseOfficeKey(const char* key, Office& out) {
  if (!key) return false;
  for (int i = 0; i < OFFICE_COUNT; ++i) {
    if (strcmp(key, OFFICE_KEYS[i]) == 0) {
      out = static_cast<Office>(i);
      return true;
    }
  }
  return false;
}

const char* officeTocTitle(const Office office) {
  const int i = static_cast<int>(office);
  return i >= 0 && i < OFFICE_COUNT ? OFFICE_TOC_TITLES[i] : "";
}

bool officeFromTocTitle(const char* title, Office& out) {
  if (!title) return false;
  // Generators may pad titles; compare without surrounding whitespace.
  while (*title == ' ' || *title == '\t' || *title == '\n') ++title;
  size_t len = strlen(title);
  while (len > 0 && (title[len - 1] == ' ' || title[len - 1] == '\t' || title[len - 1] == '\n')) --len;
  for (int i = 0; i < OFFICE_COUNT; ++i) {
    if (strlen(OFFICE_TOC_TITLES[i]) == len && strncmp(title, OFFICE_TOC_TITLES[i], len) == 0) {
      out = static_cast<Office>(i);
      return true;
    }
  }
  return false;
}

const char* officeShortName(const Office office) {
  const int i = static_cast<int>(office);
  return i >= 0 && i < OFFICE_COUNT ? OFFICE_SHORT_NAMES[i] : "";
}

Rank parseRank(const char* s) {
  if (!s) return Rank::Ferial;
  if (strcmp(s, "feast") == 0) return Rank::Feast;
  if (strcmp(s, "sunday") == 0) return Rank::Sunday;
  if (strcmp(s, "fast") == 0) return Rank::Fast;
  return Rank::Ferial;
}

bool parseClockMinutes(const char* s, uint16_t& minutes) {
  if (!s) return false;
  int h = 0;
  int m = 0;
  int i = 0;
  if (!isDigit(s[i])) return false;
  while (isDigit(s[i]) && i < 2) h = h * 10 + (s[i++] - '0');
  if (s[i++] != ':') return false;
  if (!parseDigits(s + i, 2, m)) return false;
  if (h > 23 || m > 59) return false;
  minutes = static_cast<uint16_t>(h * 60 + m);
  return true;
}

void copyField(char* dst, const size_t cap, const char* src) {
  if (cap == 0) return;
  if (!src) {
    dst[0] = '\0';
    return;
  }
  size_t n = strlen(src);
  if (n >= cap) {
    n = cap - 1;
    // Do not split a UTF-8 sequence: back up to a lead byte.
    while (n > 0 && (static_cast<unsigned char>(src[n]) & 0xC0) == 0x80) --n;
  }
  memcpy(dst, src, n);
  dst[n] = '\0';
}

}  // namespace rotunda
