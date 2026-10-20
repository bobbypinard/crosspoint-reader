#pragma once

#include <cstdint>

#include "OfficeTypes.h"
#include "TsvReader.h"

namespace rotunda {

// One row of /office/index/YYYY-MM.tsv. Fixed buffers: no heap per row, and
// overlong values are truncated rather than failing the row.
struct DayEntry {
  Date date;
  Rank rank = Rank::Ferial;
  char weekday[12] = "";
  char season[48] = "";
  char proper[32] = "";
  char year[16] = "";
  char feast[80] = "";
  char commemoration[80] = "";
  char mpPsalms[64] = "";
  char epPsalms[64] = "";
  char mpLessons[96] = "";
  char epLessons[96] = "";
  char collect[64] = "";
  char epub[112] = "";
};

// Per-day marks for the Calendar: rank and whether the index has the day.
struct MonthMarks {
  int16_t year = 0;
  uint8_t month = 0;
  uint8_t rowMask[4] = {};  // bit per day (1-based day d -> bit d-1)
  Rank rank[31] = {};

  bool hasRow(int day) const { return day >= 1 && day <= 31 && (rowMask[(day - 1) / 8] >> ((day - 1) % 8)) & 1; }
};

// Scans for one date. False if the file has no row for it.
bool findDay(LineReader& reader, const Date& date, DayEntry& out, char* line, size_t lineCap);

// Reads rank marks for every row of the month file.
void readMonthMarks(LineReader& reader, int year, int month, MonthMarks& out, char* line, size_t lineCap);

// "79 80 81" -> "79, 80, 81" (prefix is prepended, e.g. "Ps ").
void formatPsalms(const char* psalms, const char* prefix, char* out, size_t cap);
// "A; B" -> "A · B".
void joinLessons(const char* lessons, char* out, size_t cap);
// The index-th ';'-separated lesson, trimmed. False if there is none.
bool lessonAt(const char* lessons, int index, char* out, size_t cap);

// "/office/books/daily-office-1979-rite1-2026-10-16.epub" -> 2026-10-16.
bool parseOfficeEpubDate(const char* path, Date& out);
// Writes the DATA.md fallback path for a date.
void officeEpubPath(const Date& date, char* out, size_t cap);
// "/office/index/2026-10.tsv".
void indexPath(int year, int month, char* out, size_t cap);

}  // namespace rotunda
