#include "OfficeHours.h"

#include <cstdio>
#include <cstring>

namespace rotunda {

CurrentOffice currentOffice(const int hour, const int minute, const OfficeHours& hours) {
  const int now = hour * 60 + minute;
  CurrentOffice result;
  if (now < hours.dayStarts) {
    result.office = Office::Compline;
    result.previousDay = true;
  } else if (now < hours.start[static_cast<int>(Office::Noonday)]) {
    result.office = Office::Morning;
  } else if (now < hours.start[static_cast<int>(Office::Evening)]) {
    result.office = Office::Noonday;
  } else if (now < hours.start[static_cast<int>(Office::Compline)]) {
    result.office = Office::Evening;
  } else {
    result.office = Office::Compline;
  }
  return result;
}

Date officeDay(const Date& wallDate, const int hour, const int minute, const OfficeHours& hours) {
  return hour * 60 + minute < hours.dayStarts ? addDays(wallDate, -1) : wallDate;
}

void readOfficeHours(LineReader& reader, OfficeHours& hours, char* line, const size_t lineCap) {
  bool header = true;
  while (reader.next(line, lineCap)) {
    if (header) {
      header = false;
      continue;
    }
    char* fields[2];
    if (splitTsv(line, fields, 2) < 2) continue;
    uint16_t minutes = 0;
    if (!parseClockMinutes(fields[1], minutes)) continue;
    if (strcmp(fields[0], "day_starts") == 0) {
      hours.dayStarts = minutes;
      continue;
    }
    Office office;
    if (parseOfficeKey(fields[0], office)) hours.start[static_cast<int>(office)] = minutes;
  }
}

void formatHour12(const uint16_t minutes, char* out, const size_t cap) {
  int h = minutes / 60 % 12;
  if (h == 0) h = 12;
  snprintf(out, cap, "%d:%02d", h, minutes % 60);
}

void formatHour12Suffix(const uint16_t minutes, char* out, const size_t cap) {
  int h = minutes / 60 % 12;
  if (h == 0) h = 12;
  snprintf(out, cap, "%d:%02d %s", h, minutes % 60, minutes < 12 * 60 ? "am" : "pm");
}

}  // namespace rotunda
