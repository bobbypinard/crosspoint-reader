#pragma once

#include <algorithm>
#include <cstdint>
#include <iterator>

#include "OfficeTypes.h"
#include "TsvReader.h"

namespace rotunda {

// Totals and marks from one pass over /office/said.tsv. Duplicate lines are
// harmless: everything is a set bit, never a running count.
struct KeptScan {
  // Inputs.
  Date today;    // office day used for "today" and the streak
  int16_t year;  // month to collect pips for
  uint8_t month;

  // Outputs.
  uint8_t monthMask[31] = {};       // bit per Office for each day of the month
  uint8_t monthFridayMask[4] = {};  // Friday devotion kept, bit per day
  uint8_t todayMask = 0;
  bool todayFriday = false;
  uint16_t todayFinished[OFFICE_COUNT] = {};  // minutes after midnight, 0xFFFF = unknown
  // Bit i = today - i had at least one office kept. Covers a year for the streak.
  static constexpr int RECENT_DAYS = 368;
  uint8_t recent[RECENT_DAYS / 8] = {};

  KeptScan() : year(0), month(0) { std::fill(std::begin(todayFinished), std::end(todayFinished), 0xFFFF); }
  bool fridayKept(int day) const {
    return day >= 1 && day <= 31 && (monthFridayMask[(day - 1) / 8] >> ((day - 1) % 8)) & 1;
  }
};

void scanSaid(LineReader& reader, KeptScan& scan, char* line, size_t lineCap);

int officesKept(const KeptScan& scan, int throughDay);
int fullDays(const KeptScan& scan, int throughDay);
// Consecutive days with at least one office, ending today, or yesterday when
// today has nothing kept yet (the day is not over).
int streak(const KeptScan& scan);
int keptCount(uint8_t mask);

// True if said.tsv already has this office (or the Friday devotion when
// friday is true) for the date.
bool hasEntry(LineReader& reader, const Date& date, Office office, bool friday, char* line, size_t lineCap);

// One said.tsv line including '\n'. finishedMinutes < 0 writes an empty time
// (no RTC). Returns the length, or 0 if cap is too small.
int formatSaidLine(char* out, size_t cap, const Date& officeDay, Office office, bool friday, const Date& wallDate,
                   int finishedMinutes);

constexpr char SAID_HEADER[] = "date\toffice\tfinished_at\tkind\n";

}  // namespace rotunda
