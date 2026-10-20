#include "OfficeIndex.h"

#include <cstdio>
#include <cstring>

namespace rotunda {

namespace {
enum Column : uint8_t {
  COL_DATE,
  COL_WEEKDAY,
  COL_SEASON,
  COL_PROPER,
  COL_YEAR,
  COL_RANK,
  COL_FEAST,
  COL_COMMEMORATION,
  COL_MP_PSALMS,
  COL_EP_PSALMS,
  COL_MP_LESSONS,
  COL_EP_LESSONS,
  COL_COLLECT,
  COL_EPUB,
  COL_COUNT
};

constexpr const char* COLUMN_NAMES[COL_COUNT] = {"date",       "weekday",    "season",        "proper",    "year",
                                                 "rank",       "feast",      "commemoration", "mp_psalms", "ep_psalms",
                                                 "mp_lessons", "ep_lessons", "collect",       "epub"};
constexpr int MAX_FIELDS = 20;

// Column positions from the header row, so a generator may add or reorder
// columns. -1 = absent.
struct ColumnMap {
  int8_t index[COL_COUNT];

  void setDefault() {
    for (int i = 0; i < COL_COUNT; ++i) index[i] = static_cast<int8_t>(i);
  }

  void parseHeader(char* line) {
    char* fields[MAX_FIELDS];
    const int n = splitTsv(line, fields, MAX_FIELDS);
    for (int c = 0; c < COL_COUNT; ++c) {
      index[c] = -1;
      for (int f = 0; f < n; ++f) {
        if (strcmp(fields[f], COLUMN_NAMES[c]) == 0) {
          index[c] = static_cast<int8_t>(f);
          break;
        }
      }
    }
  }

  // The first line is a header unless it already starts with a date.
  bool takeHeader(char* line, bool& first) {
    first = false;
    Date ignored;
    if (parseIsoDate(line, ignored)) return false;
    parseHeader(line);
    return true;
  }

  const char* get(char* const* fields, const int count, const Column c) const {
    const int i = index[c];
    return i >= 0 && i < count ? fields[i] : "";
  }
};

void fillEntry(const ColumnMap& map, char* const* fields, const int n, const Date& date, DayEntry& out) {
  out.date = date;
  out.rank = parseRank(map.get(fields, n, COL_RANK));
  copyField(out.weekday, sizeof(out.weekday), map.get(fields, n, COL_WEEKDAY));
  copyField(out.season, sizeof(out.season), map.get(fields, n, COL_SEASON));
  copyField(out.proper, sizeof(out.proper), map.get(fields, n, COL_PROPER));
  copyField(out.year, sizeof(out.year), map.get(fields, n, COL_YEAR));
  copyField(out.feast, sizeof(out.feast), map.get(fields, n, COL_FEAST));
  copyField(out.commemoration, sizeof(out.commemoration), map.get(fields, n, COL_COMMEMORATION));
  copyField(out.mpPsalms, sizeof(out.mpPsalms), map.get(fields, n, COL_MP_PSALMS));
  copyField(out.epPsalms, sizeof(out.epPsalms), map.get(fields, n, COL_EP_PSALMS));
  copyField(out.mpLessons, sizeof(out.mpLessons), map.get(fields, n, COL_MP_LESSONS));
  copyField(out.epLessons, sizeof(out.epLessons), map.get(fields, n, COL_EP_LESSONS));
  copyField(out.collect, sizeof(out.collect), map.get(fields, n, COL_COLLECT));
  copyField(out.epub, sizeof(out.epub), map.get(fields, n, COL_EPUB));
}

// Appends src to out at *len, bounded by cap; returns false once full.
bool append(char* out, const size_t cap, size_t& len, const char* src, const size_t n) {
  if (len + 1 >= cap) return false;
  const size_t room = cap - 1 - len;
  const size_t take = n < room ? n : room;
  memcpy(out + len, src, take);
  len += take;
  out[len] = '\0';
  return take == n;
}

const char* skipSpaces(const char* p) {
  while (*p == ' ') ++p;
  return p;
}
}  // namespace

bool findDay(LineReader& reader, const Date& date, DayEntry& out, char* line, const size_t lineCap) {
  ColumnMap map;
  map.setDefault();
  bool first = true;
  while (reader.next(line, lineCap)) {
    if (first && map.takeHeader(line, first)) continue;
    char* fields[MAX_FIELDS];
    const int n = splitTsv(line, fields, MAX_FIELDS);
    Date rowDate;
    if (!parseIsoDate(map.get(fields, n, COL_DATE), rowDate) || rowDate != date) continue;
    fillEntry(map, fields, n, rowDate, out);
    return true;
  }
  return false;
}

void readMonthMarks(LineReader& reader, const int year, const int month, MonthMarks& out, char* line,
                    const size_t lineCap) {
  out = MonthMarks{};
  out.year = static_cast<int16_t>(year);
  out.month = static_cast<uint8_t>(month);
  ColumnMap map;
  map.setDefault();
  bool first = true;
  while (reader.next(line, lineCap)) {
    if (first && map.takeHeader(line, first)) continue;
    char* fields[MAX_FIELDS];
    const int n = splitTsv(line, fields, MAX_FIELDS);
    Date rowDate;
    if (!parseIsoDate(map.get(fields, n, COL_DATE), rowDate) || rowDate.year != year || rowDate.month != month) {
      continue;
    }
    const int d = rowDate.day;
    out.rowMask[(d - 1) / 8] |= static_cast<uint8_t>(1u << ((d - 1) % 8));
    out.rank[d - 1] = parseRank(map.get(fields, n, COL_RANK));
  }
}

void formatPsalms(const char* psalms, const char* prefix, char* out, const size_t cap) {
  if (cap == 0) return;
  out[0] = '\0';
  size_t len = 0;
  if (!psalms || !*psalms) return;
  if (prefix) append(out, cap, len, prefix, strlen(prefix));
  const char* p = skipSpaces(psalms);
  bool first = true;
  while (*p) {
    const char* end = p;
    while (*end && *end != ' ') ++end;
    if (!first && !append(out, cap, len, ", ", 2)) return;
    if (!append(out, cap, len, p, static_cast<size_t>(end - p))) return;
    first = false;
    p = skipSpaces(end);
  }
}

void joinLessons(const char* lessons, char* out, const size_t cap) {
  if (cap == 0) return;
  out[0] = '\0';
  if (!lessons) return;
  size_t len = 0;
  static constexpr char SEP[] = " \xC2\xB7 ";  // " · "
  for (int i = 0;; ++i) {
    char one[96];
    if (!lessonAt(lessons, i, one, sizeof(one))) break;
    if (i > 0 && !append(out, cap, len, SEP, sizeof(SEP) - 1)) return;
    if (!append(out, cap, len, one, strlen(one))) return;
  }
}

bool lessonAt(const char* lessons, const int index, char* out, const size_t cap) {
  if (cap == 0) return false;
  out[0] = '\0';
  if (!lessons) return false;
  const char* p = lessons;
  for (int i = 0; i < index; ++i) {
    p = strchr(p, ';');
    if (!p) return false;
    ++p;
  }
  p = skipSpaces(p);
  const char* end = strchr(p, ';');
  size_t n = end ? static_cast<size_t>(end - p) : strlen(p);
  while (n > 0 && p[n - 1] == ' ') --n;
  if (n == 0) return false;
  size_t len = 0;
  append(out, cap, len, p, n);
  return true;
}

bool parseOfficeEpubDate(const char* path, Date& out) {
  if (!path) return false;
  static constexpr char STEM[] = "daily-office-1979-rite1-";
  const char* base = strrchr(path, '/');
  base = base ? base + 1 : path;
  if (strncmp(base, STEM, sizeof(STEM) - 1) != 0) return false;
  const char* date = base + sizeof(STEM) - 1;
  if (strlen(date) < 15 || strcmp(date + 10, ".epub") != 0) return false;
  return parseIsoDate(date, out);
}

void officeEpubPath(const Date& date, char* out, const size_t cap) {
  snprintf(out, cap, "/office/books/daily-office-1979-rite1-%04d-%02d-%02d.epub", date.year, date.month, date.day);
}

void indexPath(const int year, const int month, char* out, const size_t cap) {
  snprintf(out, cap, "/office/index/%04d-%02d.tsv", year, month);
}

}  // namespace rotunda
