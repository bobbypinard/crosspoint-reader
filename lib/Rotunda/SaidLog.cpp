#include "SaidLog.h"

#include <cstdio>
#include <cstring>

namespace rotunda {

namespace {
struct SaidRow {
  Date date;
  Office office = Office::Morning;
  bool friday = false;
  int finishedMinutes = -1;
};

bool parseRow(char* line, SaidRow& row) {
  char* fields[4];
  const int n = splitTsv(line, fields, 4);
  if (n < 2 || !parseIsoDate(fields[0], row.date)) return false;
  if (strcmp(fields[1], "friday") == 0) {
    row.friday = true;
  } else if (!parseOfficeKey(fields[1], row.office)) {
    return false;
  }
  row.finishedMinutes = -1;
  if (n >= 3) {
    // "2026-10-16T08:41": the time follows the 'T'.
    const char* t = strchr(fields[2], 'T');
    uint16_t minutes = 0;
    if (t && parseClockMinutes(t + 1, minutes)) row.finishedMinutes = minutes;
  }
  return true;
}
}  // namespace

void scanSaid(LineReader& reader, KeptScan& scan, char* line, const size_t lineCap) {
  const int32_t todayNumber = dayNumber(scan.today);
  while (reader.next(line, lineCap)) {
    SaidRow row;
    if (!parseRow(line, row)) continue;  // header and malformed lines
    const int d = row.date.day;
    const bool inMonth = row.date.year == scan.year && row.date.month == scan.month;
    if (row.friday) {
      if (inMonth) scan.monthFridayMask[(d - 1) / 8] |= static_cast<uint8_t>(1u << ((d - 1) % 8));
      if (row.date == scan.today) scan.todayFriday = true;
      continue;
    }
    const auto bit = static_cast<uint8_t>(1u << static_cast<int>(row.office));
    if (inMonth) scan.monthMask[d - 1] |= bit;
    if (row.date == scan.today) {
      scan.todayMask |= bit;
      auto& finished = scan.todayFinished[static_cast<int>(row.office)];
      // Keep the first finish time when an office was read twice.
      if (row.finishedMinutes >= 0 && (finished == 0xFFFF || row.finishedMinutes < finished)) {
        finished = static_cast<uint16_t>(row.finishedMinutes);
      }
    }
    const int32_t age = todayNumber - dayNumber(row.date);
    if (age >= 0 && age < KeptScan::RECENT_DAYS) scan.recent[age / 8] |= static_cast<uint8_t>(1u << (age % 8));
  }
}

int keptCount(uint8_t mask) {
  int n = 0;
  for (; mask; mask &= static_cast<uint8_t>(mask - 1)) ++n;
  return n;
}

int officesKept(const KeptScan& scan, const int throughDay) {
  int total = 0;
  for (int d = 1; d <= throughDay && d <= 31; ++d) total += keptCount(scan.monthMask[d - 1]);
  return total;
}

int fullDays(const KeptScan& scan, const int throughDay) {
  constexpr uint8_t ALL = (1u << OFFICE_COUNT) - 1;
  int total = 0;
  for (int d = 1; d <= throughDay && d <= 31; ++d) total += (scan.monthMask[d - 1] & ALL) == ALL ? 1 : 0;
  return total;
}

int streak(const KeptScan& scan) {
  const auto kept = [&scan](const int age) { return (scan.recent[age / 8] >> (age % 8)) & 1; };
  int age = kept(0) ? 0 : 1;
  int count = 0;
  while (age < KeptScan::RECENT_DAYS && kept(age)) {
    ++count;
    ++age;
  }
  return count;
}

bool hasEntry(LineReader& reader, const Date& date, const Office office, const bool friday, char* line,
              const size_t lineCap) {
  while (reader.next(line, lineCap)) {
    SaidRow row;
    if (!parseRow(line, row) || row.date != date || row.friday != friday) continue;
    if (friday || row.office == office) return true;
  }
  return false;
}

int formatSaidLine(char* out, const size_t cap, const Date& officeDay, const Office office, const bool friday,
                   const Date& wallDate, const int finishedMinutes) {
  char day[11];
  formatIsoDate(officeDay, day, sizeof(day));
  char finished[24] = "";
  if (finishedMinutes >= 0) {
    char wall[11];
    formatIsoDate(wallDate, wall, sizeof(wall));
    snprintf(finished, sizeof(finished), "%sT%02d:%02d", wall, finishedMinutes / 60 % 24, finishedMinutes % 60);
  }
  const int n = snprintf(out, cap, "%s\t%s\t%s\t%s\n", day, friday ? "friday" : officeKey(office), finished,
                         friday ? "devotion" : "office");
  return n > 0 && static_cast<size_t>(n) < cap ? n : 0;
}

}  // namespace rotunda
