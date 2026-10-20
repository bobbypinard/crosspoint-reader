#pragma once

#include <Clubs.h>
#include <OfficeHours.h>
#include <OfficeIndex.h>
#include <OfficeTypes.h>
#include <SaidLog.h>

#include <cstddef>

// SD-card side of the Rotunda data module: opens the DATA.md files through
// HalStorage and feeds them to the host-tested parsers in lib/Rotunda.
// Each call borrows one 640-byte scratch block from the heap for its file
// buffers and frees it before returning; nothing is held between calls
// except the office hours.
namespace RotundaStore {

constexpr char SETTINGS_PATH[] = "/office/settings.tsv";
constexpr char SAID_PATH[] = "/office/said.tsv";
constexpr char CLUBS_PATH[] = "/clubs/clubs.tsv";
constexpr char BOOKS_DIR[] = "/office/books";
constexpr char PSALTER_PATH[] = "/office/psalter.epub";

struct Now {
  bool hasClock = false;
  rotunda::Date wallDate;
  int hour = 0;
  int minute = 0;
  rotunda::Date officeDay;  // wallDate, or the day before between midnight and day_starts
  rotunda::CurrentOffice current;
};

// Re-reads /office/settings.tsv (defaults when absent). Cheap; call on screen entry.
void reloadHours();
const rotunda::OfficeHours& hours();

// Clock-derived "now". Without an RTC: Morning of the newest office EPUB's date
// (DESIGN.md: fall back to Morning and show no clock).
Now now();

bool findDay(const rotunda::Date& date, rotunda::DayEntry& out);
// False when the month has no index file (marks stay empty).
bool readMonthMarks(int year, int month, rotunda::MonthMarks& out);
// scan.today/year/month must be set. False when said.tsv is absent (scan stays empty).
bool scanSaid(rotunda::KeptScan& scan);
bool readClubs(rotunda::ClubList& out);

// Appends one line to said.tsv unless that office (or the Friday devotion) is
// already logged for officeDay. Writes the header when creating the file.
bool appendKept(const rotunda::Date& officeDay, rotunda::Office office, bool friday);

// The day's EPUB: the index path if that file exists, else the DATA.md
// filename pattern. False (out empty) when neither is on the card.
bool resolveDayEpub(const rotunda::DayEntry* entry, const rotunda::Date& date, char* out, size_t cap);

}  // namespace RotundaStore
