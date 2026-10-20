#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "Clubs.h"
#include "OfficeHours.h"
#include "OfficeIndex.h"
#include "OfficeToc.h"
#include "OfficeTypes.h"
#include "SaidLog.h"
#include "TsvReader.h"

using namespace rotunda;

namespace {

class FileSource : public ByteSource {
 public:
  explicit FileSource(const std::string& path) : f(fopen(path.c_str(), "rb")) {}
  ~FileSource() override {
    if (f) fclose(f);
  }
  bool ok() const { return f != nullptr; }
  size_t read(char* buf, size_t cap) override { return f ? fread(buf, 1, cap, f) : 0; }

 private:
  FILE* f;
};

class StringSource : public ByteSource {
 public:
  explicit StringSource(std::string s) : data(std::move(s)) {}
  size_t read(char* buf, size_t cap) override {
    const size_t n = std::min(cap, data.size() - pos);
    memcpy(buf, data.data() + pos, n);
    pos += n;
    return n;
  }

 private:
  std::string data;
  size_t pos = 0;
};

std::string sample(const char* rel) { return std::string(ROTUNDA_SAMPLE_DIR) + rel; }

Date date(int y, int m, int d) {
  Date out;
  out.year = static_cast<int16_t>(y);
  out.month = static_cast<uint8_t>(m);
  out.day = static_cast<uint8_t>(d);
  return out;
}

// Same buffer sizes RotundaStore uses on the device.
struct Buffers {
  char chunk[128];
  char line[512];
};

}  // namespace

TEST(RotundaDates, ParseFormatAndArithmetic) {
  Date d;
  ASSERT_TRUE(parseIsoDate("2026-10-16", d));
  EXPECT_EQ(d, date(2026, 10, 16));
  EXPECT_FALSE(parseIsoDate("2026-02-30", d));
  EXPECT_FALSE(parseIsoDate("date", d));
  char buf[11];
  formatIsoDate(date(2026, 1, 5), buf, sizeof(buf));
  EXPECT_STREQ(buf, "2026-01-05");
  EXPECT_EQ(weekday(date(2026, 10, 16)), 5);  // Friday
  EXPECT_EQ(weekday(date(2026, 10, 18)), 0);  // Sunday
  EXPECT_EQ(addDays(date(2026, 10, 1), -1), date(2026, 9, 30));
  EXPECT_EQ(addDays(date(2026, 12, 31), 1), date(2027, 1, 1));
  EXPECT_EQ(daysInMonth(2028, 2), 29);
  EXPECT_EQ(dayNumber(date(1970, 1, 1)), 0);
}

TEST(RotundaOffices, KeysAndTocTitles) {
  Office o;
  ASSERT_TRUE(officeFromTocTitle("Evening Prayer", o));
  EXPECT_EQ(o, Office::Evening);
  ASSERT_TRUE(officeFromTocTitle("  Compline \n", o));
  EXPECT_EQ(o, Office::Compline);
  EXPECT_FALSE(officeFromTocTitle("Psalm 86", o));
  ASSERT_TRUE(parseOfficeKey("noonday", o));
  EXPECT_EQ(o, Office::Noonday);
  EXPECT_STREQ(officeKey(Office::Morning), "morning");
}

TEST(RotundaHours, CurrentOfficeWindows) {
  const OfficeHours h;
  const auto at = [&h](int hh, int mm) { return currentOffice(hh, mm, h); };
  EXPECT_EQ(at(4, 0).office, Office::Morning);
  EXPECT_EQ(at(11, 59).office, Office::Morning);
  EXPECT_EQ(at(12, 0).office, Office::Noonday);
  EXPECT_EQ(at(12, 13).office, Office::Noonday);
  EXPECT_EQ(at(16, 29).office, Office::Noonday);
  EXPECT_EQ(at(16, 30).office, Office::Evening);
  EXPECT_EQ(at(19, 59).office, Office::Evening);
  EXPECT_EQ(at(20, 0).office, Office::Compline);
  EXPECT_FALSE(at(23, 59).previousDay);
  EXPECT_EQ(at(0, 0).office, Office::Compline);
  EXPECT_TRUE(at(0, 0).previousDay);
  EXPECT_TRUE(at(3, 59).previousDay);
  EXPECT_EQ(officeDay(date(2026, 10, 18), 0, 30, h), date(2026, 10, 17));
  EXPECT_EQ(officeDay(date(2026, 10, 1), 3, 59, h), date(2026, 9, 30));
  EXPECT_EQ(officeDay(date(2026, 10, 18), 4, 0, h), date(2026, 10, 18));
}

TEST(RotundaHours, ReadsSettingsAndKeepsDefaults) {
  FileSource src(sample("/office/settings.tsv"));
  ASSERT_TRUE(src.ok());
  Buffers b;
  LineReader reader(src, b.chunk, sizeof(b.chunk));
  OfficeHours h;
  readOfficeHours(reader, h, b.line, sizeof(b.line));
  EXPECT_EQ(h.start[0], 8 * 60 + 30);
  EXPECT_EQ(h.start[3], 20 * 60);
  EXPECT_EQ(h.dayStarts, 4 * 60);

  StringSource custom("key\tvalue\r\nmorning\t07:15\r\nbogus\t1\r\nevening\tlate\r\nday_starts\t3:30\r\n");
  LineReader r2(custom, b.chunk, sizeof(b.chunk));
  OfficeHours h2;
  readOfficeHours(r2, h2, b.line, sizeof(b.line));
  EXPECT_EQ(h2.start[0], 7 * 60 + 15);
  EXPECT_EQ(h2.start[2], 16 * 60 + 30);  // malformed value keeps default
  EXPECT_EQ(h2.dayStarts, 3 * 60 + 30);
  EXPECT_EQ(currentOffice(3, 45, h2).office, Office::Morning);

  char buf[12];
  formatHour12(16 * 60 + 30, buf, sizeof(buf));
  EXPECT_STREQ(buf, "4:30");
  formatHour12Suffix(12 * 60, buf, sizeof(buf));
  EXPECT_STREQ(buf, "12:00 pm");
}

TEST(RotundaIndex, FindsOneDay) {
  FileSource src(sample("/office/index/2026-10.tsv"));
  ASSERT_TRUE(src.ok());
  Buffers b;
  LineReader reader(src, b.chunk, sizeof(b.chunk));
  DayEntry e;
  ASSERT_TRUE(findDay(reader, date(2026, 10, 16), e, b.line, sizeof(b.line)));
  EXPECT_STREQ(e.weekday, "Friday");
  EXPECT_STREQ(e.proper, "Proper 23");
  EXPECT_STREQ(e.year, "Year Two");
  EXPECT_EQ(e.rank, Rank::Fast);
  EXPECT_STREQ(e.feast, "");
  EXPECT_STREQ(e.commemoration, "Hugh Latimer, Nicholas Ridley & Thomas Cranmer");
  EXPECT_STREQ(e.mpPsalms, "79 80 81");
  EXPECT_STREQ(e.epLessons,
               "Luke 9:28\xE2\x80\x93"
               "36");
  EXPECT_STREQ(e.epub, "/office/books/daily-office-1979-rite1-2026-10-16.epub");

  FileSource src2(sample("/office/index/2026-10.tsv"));
  LineReader r2(src2, b.chunk, sizeof(b.chunk));
  ASSERT_TRUE(findDay(r2, date(2026, 10, 19), e, b.line, sizeof(b.line)));
  EXPECT_EQ(e.rank, Rank::Feast);
  EXPECT_STREQ(e.feast, "Saint Luke the Evangelist (transferred from 18 Oct)");

  FileSource src3(sample("/office/index/2026-10.tsv"));
  LineReader r3(src3, b.chunk, sizeof(b.chunk));
  EXPECT_FALSE(findDay(r3, date(2026, 11, 1), e, b.line, sizeof(b.line)));
}

TEST(RotundaIndex, ColumnsFollowTheHeader) {
  StringSource src("epub\tdate\trank\tproper\n/x.epub\t2026-10-02\tfast\tProper 21\n");
  Buffers b;
  LineReader reader(src, b.chunk, sizeof(b.chunk));
  DayEntry e;
  ASSERT_TRUE(findDay(reader, date(2026, 10, 2), e, b.line, sizeof(b.line)));
  EXPECT_STREQ(e.epub, "/x.epub");
  EXPECT_STREQ(e.proper, "Proper 21");
  EXPECT_EQ(e.rank, Rank::Fast);
  EXPECT_STREQ(e.season, "");
}

TEST(RotundaIndex, MonthMarks) {
  FileSource src(sample("/office/index/2026-10.tsv"));
  Buffers b;
  LineReader reader(src, b.chunk, sizeof(b.chunk));
  MonthMarks m;
  readMonthMarks(reader, 2026, 10, m, b.line, sizeof(b.line));
  for (int d = 1; d <= 31; ++d) EXPECT_TRUE(m.hasRow(d)) << d;
  EXPECT_EQ(m.rank[2 - 1], Rank::Fast);
  EXPECT_EQ(m.rank[9 - 1], Rank::Fast);
  EXPECT_EQ(m.rank[19 - 1], Rank::Feast);
  EXPECT_EQ(m.rank[23 - 1], Rank::Feast);
  EXPECT_EQ(m.rank[28 - 1], Rank::Feast);
  EXPECT_EQ(m.rank[30 - 1], Rank::Fast);
  EXPECT_EQ(m.rank[18 - 1], Rank::Sunday);
  EXPECT_EQ(m.rank[17 - 1], Rank::Ferial);
}

TEST(RotundaIndex, FormatsPsalmsAndLessons) {
  char out[96];
  formatPsalms("86 87 88", "Ps ", out, sizeof(out));
  EXPECT_STREQ(out, "Ps 86, 87, 88");
  formatPsalms(
      "119:105\xE2\x80\x93"
      "144",
      nullptr, out, sizeof(out));
  EXPECT_STREQ(out,
               "119:105\xE2\x80\x93"
               "144");
  formatPsalms("", "Ps ", out, sizeof(out));
  EXPECT_STREQ(out, "");
  joinLessons("Ecclus. 3:17-31; Acts 28:17-31", out, sizeof(out));
  EXPECT_STREQ(out, "Ecclus. 3:17-31 \xC2\xB7 Acts 28:17-31");
  ASSERT_TRUE(lessonAt("Ecclus. 1:1-10, 18-27; Acts 28:1-16", 1, out, sizeof(out)));
  EXPECT_STREQ(out, "Acts 28:1-16");
  EXPECT_FALSE(lessonAt("Luke 9:28-36", 1, out, sizeof(out)));
  char tiny[8];
  formatPsalms("120 121 122 123", "Ps ", tiny, sizeof(tiny));
  EXPECT_EQ(strlen(tiny), 7u);
}

TEST(RotundaIndex, EpubNamesAndPaths) {
  Date d;
  ASSERT_TRUE(parseOfficeEpubDate("/office/books/daily-office-1979-rite1-2026-10-16.epub", d));
  EXPECT_EQ(d, date(2026, 10, 16));
  EXPECT_FALSE(parseOfficeEpubDate("/Books/Silence.epub", d));
  EXPECT_FALSE(parseOfficeEpubDate("/office/books/daily-office-1979-rite1-2026-10-16.epub.bak", d));
  char path[96];
  officeEpubPath(date(2026, 10, 2), path, sizeof(path));
  EXPECT_STREQ(path, "/office/books/daily-office-1979-rite1-2026-10-02.epub");
  indexPath(2026, 9, path, sizeof(path));
  EXPECT_STREQ(path, "/office/index/2026-09.tsv");
}

TEST(RotundaSaid, MonthTotalsMatchTheCalendarRender) {
  FileSource src(sample("/office/said.tsv"));
  ASSERT_TRUE(src.ok());
  Buffers b;
  LineReader reader(src, b.chunk, sizeof(b.chunk));
  KeptScan scan;
  scan.today = date(2026, 10, 17);
  scan.year = 2026;
  scan.month = 10;
  scanSaid(reader, scan, b.line, sizeof(b.line));
  // screens/calendar.png: 55 of 68, 9 full days, 17-day streak, Fridays 3 of 3.
  EXPECT_EQ(officesKept(scan, 17), 55);
  EXPECT_EQ(fullDays(scan, 17), 9);
  EXPECT_EQ(streak(scan), 17);
  EXPECT_TRUE(scan.fridayKept(2));
  EXPECT_TRUE(scan.fridayKept(9));
  EXPECT_TRUE(scan.fridayKept(16));
  EXPECT_FALSE(scan.fridayKept(30));
  EXPECT_EQ(keptCount(scan.monthMask[3 - 1]), 2);
  EXPECT_EQ(scan.todayMask, 1u << static_cast<int>(Office::Morning));
  EXPECT_EQ(scan.todayFinished[0], 8 * 60 + 41);
  EXPECT_EQ(scan.todayFinished[1], 0xFFFF);
}

TEST(RotundaSaid, StreakWaitsForTodayAndIgnoresDuplicates) {
  StringSource src(
      "date\toffice\tfinished_at\tkind\n"
      "2026-10-15\tmorning\t2026-10-15T08:41\toffice\n"
      "2026-10-16\tevening\t2026-10-16T16:52\toffice\n"
      "2026-10-16\tevening\t2026-10-16T17:30\toffice\n"
      "2026-10-17\tcompline\t2026-10-18T00:20\toffice\n"
      "garbage line\n");
  Buffers b;
  LineReader reader(src, b.chunk, sizeof(b.chunk));
  KeptScan scan;
  scan.today = date(2026, 10, 18);
  scan.year = 2026;
  scan.month = 10;
  scanSaid(reader, scan, b.line, sizeof(b.line));
  EXPECT_EQ(streak(scan), 3);  // today has nothing yet; 15-17 count
  EXPECT_EQ(officesKept(scan, 18), 3);
  EXPECT_EQ(scan.todayMask, 0);
}

TEST(RotundaSaid, DuplicateCheckAndLineFormat) {
  Buffers b;
  {
    FileSource src(sample("/office/said.tsv"));
    LineReader reader(src, b.chunk, sizeof(b.chunk));
    EXPECT_TRUE(hasEntry(reader, date(2026, 10, 17), Office::Morning, false, b.line, sizeof(b.line)));
  }
  {
    FileSource src(sample("/office/said.tsv"));
    LineReader reader(src, b.chunk, sizeof(b.chunk));
    EXPECT_FALSE(hasEntry(reader, date(2026, 10, 17), Office::Noonday, false, b.line, sizeof(b.line)));
  }
  {
    FileSource src(sample("/office/said.tsv"));
    LineReader reader(src, b.chunk, sizeof(b.chunk));
    EXPECT_TRUE(hasEntry(reader, date(2026, 10, 16), Office::Morning, true, b.line, sizeof(b.line)));
  }
  char line[64];
  ASSERT_GT(formatSaidLine(line, sizeof(line), date(2026, 10, 17), Office::Compline, false, date(2026, 10, 18), 20), 0);
  EXPECT_STREQ(line, "2026-10-17\tcompline\t2026-10-18T00:20\toffice\n");
  ASSERT_GT(formatSaidLine(line, sizeof(line), date(2026, 10, 16), Office::Morning, true, date(2026, 10, 16), -1), 0);
  EXPECT_STREQ(line, "2026-10-16\tfriday\t\tdevotion\n");
  EXPECT_EQ(formatSaidLine(line, 10, date(2026, 10, 16), Office::Morning, false, date(2026, 10, 16), 0), 0);
}

TEST(RotundaClubs, ReadsSampleClubs) {
  FileSource src(sample("/clubs/clubs.tsv"));
  ASSERT_TRUE(src.ok());
  Buffers b;
  LineReader reader(src, b.chunk, sizeof(b.chunk));
  ClubList clubs;
  readClubs(reader, clubs, b.line, sizeof(b.line));
  ASSERT_EQ(clubs.count, 3);
  EXPECT_STREQ(clubs.clubs[0].name, "Bonhoeffer group");
  EXPECT_STREQ(clubs.clubs[0].book, "/Books/The Cost of Discipleship.epub");
  EXPECT_EQ(clubs.clubs[0].startToc, 3);
  EXPECT_EQ(clubs.clubs[0].endToc, 5);
  EXPECT_EQ(clubs.clubs[0].meets, date(2026, 10, 22));
  EXPECT_EQ(clubs.clubs[0].meetsMinutes, 19 * 60);
  EXPECT_STREQ(clubs.clubs[0].notes, "/clubs/bonhoeffer-prep.txt");
  EXPECT_EQ(clubs.clubs[2].startToc, 0);  // blank assignment range
  EXPECT_EQ(clubs.clubs[2].endToc, 0);
  EXPECT_STREQ(clubs.clubs[2].assignment, "Finished \xC2\xB7 next pick TBD");
}

TEST(RotundaClubs, SkipsBadSlotsAndSorts) {
  StringSource src(
      "slot\tname\tbook\n"
      "3\tC\t/c.epub\n"
      "1\tA\t/a.epub\n"
      "9\tBad\t/x.epub\n"
      "1\tDup\t/d.epub\n"
      "2\tB\n");
  Buffers b;
  LineReader reader(src, b.chunk, sizeof(b.chunk));
  ClubList clubs;
  readClubs(reader, clubs, b.line, sizeof(b.line));
  ASSERT_EQ(clubs.count, 2);
  EXPECT_STREQ(clubs.clubs[0].name, "A");
  EXPECT_STREQ(clubs.clubs[1].name, "C");
  EXPECT_STREQ(clubs.clubs[1].notes, "");
}

TEST(RotundaTsv, LongLinesAreTruncatedNotSplit) {
  std::string longLine(700, 'x');
  StringSource src(longLine + "\nnext\n");
  Buffers b;
  LineReader reader(src, b.chunk, sizeof(b.chunk));
  ASSERT_TRUE(reader.next(b.line, sizeof(b.line)));
  EXPECT_EQ(strlen(b.line), sizeof(b.line) - 1);
  ASSERT_TRUE(reader.next(b.line, sizeof(b.line)));
  EXPECT_STREQ(b.line, "next");
  EXPECT_FALSE(reader.next(b.line, sizeof(b.line)));
}

// Each office in its own spine item; psalms are anchored children.
TEST(RotundaOfficeToc, OfficesInSeparateSpineItems) {
  OfficeToc toc;
  toc.add("Title page", 0, 0, true);
  toc.add("Morning Prayer", 1, 0, true);
  toc.add("Psalm 86", 1, 1, false);
  toc.add("Psalm 87", 1, 1, false);
  toc.add("Psalm 88", 1, 1, false);
  toc.add("Noonday Prayer", 2, 0, true);
  toc.add("Evening Prayer", 3, 0, true);
  toc.add("Compline", 4, 0, true);
  toc.add("Colophon", 5, 0, true);
  constexpr int spines = 6;

  toc.resetBounds(1);
  toc.addBound(2, 1);
  toc.addBound(3, 4);
  toc.addBound(4, 7);
  EXPECT_EQ(toc.officeAtPage(1, 0), static_cast<int>(Office::Morning));
  EXPECT_EQ(toc.tocAtPage(1, 0), 1);
  EXPECT_EQ(toc.tocAtPage(1, 5), 3);
  EXPECT_FALSE(toc.isOfficeEnd(1, 5, 10, true, spines));
  EXPECT_FALSE(toc.isOfficeEnd(1, 9, 10, false, spines));  // more pages may still be laid out
  EXPECT_TRUE(toc.isOfficeEnd(1, 9, 10, true, spines));

  int index = 0;
  int total = 0;
  toc.siblingPosition(3, index, total);
  EXPECT_EQ(index, 2);
  EXPECT_EQ(total, 3);
  toc.siblingPosition(1, index, total);  // the office heading itself
  EXPECT_EQ(total, 0);

  toc.resetBounds(0);
  EXPECT_EQ(toc.officeAtPage(0, 0), -1);
  EXPECT_FALSE(toc.isOfficeEnd(0, 0, 1, true, spines));

  toc.resetBounds(4);
  EXPECT_EQ(toc.officeAtPage(4, 2), static_cast<int>(Office::Compline));
  EXPECT_TRUE(toc.isOfficeEnd(4, 2, 3, true, spines));  // the colophon is not an office
  EXPECT_EQ(toc.office(8), -1);
}

// All four offices in one spine item, found by anchors.
TEST(RotundaOfficeToc, OfficesShareOneSpineItem) {
  OfficeToc toc;
  toc.add("Morning Prayer", 1, 0, false);
  toc.add("Noonday Prayer", 1, 0, false);
  toc.add("Evening Prayer", 1, 0, false);
  toc.add("Compline", 1, 0, false);
  constexpr int spines = 2;

  toc.resetBounds(1);
  EXPECT_EQ(toc.officeAtPage(1, 3), -1);  // nothing resolved yet: never log
  toc.addBound(0, 0);
  toc.addBound(1, 12);
  toc.addBound(2, 16);
  toc.addBound(3, 28);
  EXPECT_FALSE(toc.isOfficeEnd(1, 10, 33, false, spines));
  EXPECT_TRUE(toc.isOfficeEnd(1, 11, 33, false, spines));
  EXPECT_TRUE(toc.isOfficeEnd(1, 15, 33, false, spines));
  EXPECT_TRUE(toc.isOfficeEnd(1, 27, 33, false, spines));
  EXPECT_FALSE(toc.isOfficeEnd(1, 32, 33, false, spines));
  EXPECT_TRUE(toc.isOfficeEnd(1, 32, 33, true, spines));  // end of the book
  EXPECT_EQ(toc.officeAtPage(1, 20), static_cast<int>(Office::Evening));
}

// An office that runs on into the next spine item is not finished at the item's end.
TEST(RotundaOfficeToc, OfficeSpanningSpineItems) {
  OfficeToc toc;
  toc.add("Morning Prayer", 1, 0, true);
  toc.add("The Lessons", 2, 1, true);
  toc.add("Evening Prayer", 3, 0, true);
  toc.resetBounds(1);
  EXPECT_FALSE(toc.isOfficeEnd(1, 4, 5, true, 4));
  toc.resetBounds(2);
  EXPECT_TRUE(toc.isOfficeEnd(2, 6, 7, true, 4));

  OfficeToc untitled;  // the continuation item has no TOC entry at all
  untitled.add("Morning Prayer", 1, 0, true);
  untitled.add("Evening Prayer", 3, 0, true);
  untitled.resetBounds(1);
  EXPECT_FALSE(untitled.isOfficeEnd(1, 4, 5, true, 4));
  untitled.resetBounds(2);
  EXPECT_EQ(untitled.officeAtPage(2, 0), static_cast<int>(Office::Morning));
  EXPECT_TRUE(untitled.isOfficeEnd(2, 3, 4, true, 4));
}
