#pragma once

#include <cstdint>

#include "OfficeTypes.h"
#include "TsvReader.h"

namespace rotunda {

// /office/settings.tsv. Defaults are the DATA.md values.
struct OfficeHours {
  // Displayed hour of each office, minutes after midnight.
  uint16_t start[OFFICE_COUNT] = {8 * 60 + 30, 12 * 60, 16 * 60 + 30, 20 * 60};
  // Morning's window opens here; earlier times are the previous day's Compline.
  uint16_t dayStarts = 4 * 60;
};

struct CurrentOffice {
  Office office = Office::Morning;
  // True when the clock is past midnight but before dayStarts.
  bool previousDay = false;
};

// Windows from DESIGN.md: Morning dayStarts..noonday, Noonday ..evening,
// Evening ..compline, Compline ..next dayStarts (counts toward the day it began).
CurrentOffice currentOffice(int hour, int minute, const OfficeHours& hours);

// The office day for a wall-clock date and time.
Date officeDay(const Date& wallDate, int hour, int minute, const OfficeHours& hours);

// Reads key/value rows; unknown keys and malformed values keep the defaults.
void readOfficeHours(LineReader& reader, OfficeHours& hours, char* line, size_t lineCap);

// "8:30", "12:00", "4:30", "8:00" (12-hour without suffix, as drawn on the screens).
void formatHour12(uint16_t minutes, char* out, size_t cap);
// "8:30 am", "12:00 pm" (Today's Office rows).
void formatHour12Suffix(uint16_t minutes, char* out, size_t cap);

}  // namespace rotunda
