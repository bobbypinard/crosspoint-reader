#include "RotundaStore.h"

#include <HalClock.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>
#include <TsvReader.h>

#include <cstring>
#include <ctime>

namespace RotundaStore {

namespace {

rotunda::OfficeHours officeHours;

class HalByteSource final : public rotunda::ByteSource {
 public:
  explicit HalByteSource(HalFile& file) : file(file) {}
  size_t read(char* buf, const size_t cap) override {
    const int n = file.read(buf, cap);
    return n > 0 ? static_cast<size_t>(n) : 0;
  }

 private:
  HalFile& file;
};

struct Scratch {
  char chunk[128];
  char line[512];
};

// Opens path and runs fn(reader, line, lineCap). False if the file is absent
// or the scratch block cannot be allocated.
template <typename Fn>
bool withReader(const char* path, Fn&& fn) {
  if (!Storage.exists(path)) return false;
  HalFile file;
  if (!Storage.openFileForRead("ROT", path, file)) return false;
  auto scratch = makeUniqueNoThrow<Scratch>();
  if (!scratch) {
    LOG_ERR("ROT", "OOM: TSV scratch (%u bytes)", static_cast<unsigned>(sizeof(Scratch)));
    return false;
  }
  HalByteSource source(file);
  rotunda::LineReader reader(source, scratch->chunk, sizeof(scratch->chunk));
  fn(reader, scratch->line, sizeof(scratch->line));
  return true;
}

// Newest daily-office EPUB date in /office/books, for boards without an RTC.
bool newestOfficeEpubDate(rotunda::Date& out) {
  HalFile dir = Storage.open(BOOKS_DIR);
  if (!dir || !dir.isDirectory()) return false;
  bool found = false;
  char name[96];
  for (HalFile entry = dir.openNextFile(); entry; entry = dir.openNextFile()) {
    entry.getName(name, sizeof(name));
    rotunda::Date d;
    if (rotunda::parseOfficeEpubDate(name, d) && (!found || rotunda::dayNumber(d) > rotunda::dayNumber(out))) {
      out = d;
      found = true;
    }
  }
  return found;
}

}  // namespace

void reloadHours() {
  officeHours = rotunda::OfficeHours{};
  withReader(SETTINGS_PATH, [](rotunda::LineReader& reader, char* line, const size_t cap) {
    rotunda::readOfficeHours(reader, officeHours, line, cap);
  });
}

const rotunda::OfficeHours& hours() { return officeHours; }

Now now() {
  Now result;
  struct tm t{};
  if (halClock.isAvailable() && halClock.localTime(t) && t.tm_year >= 100) {
    result.hasClock = true;
    result.wallDate.year = static_cast<int16_t>(t.tm_year + 1900);
    result.wallDate.month = static_cast<uint8_t>(t.tm_mon + 1);
    result.wallDate.day = static_cast<uint8_t>(t.tm_mday);
    result.hour = t.tm_hour;
    result.minute = t.tm_min;
    result.officeDay = rotunda::officeDay(result.wallDate, result.hour, result.minute, officeHours);
    result.current = rotunda::currentOffice(result.hour, result.minute, officeHours);
    return result;
  }
  rotunda::Date fallback;
  if (newestOfficeEpubDate(fallback)) {
    result.wallDate = fallback;
    result.officeDay = fallback;
  }
  result.current.office = rotunda::Office::Morning;
  return result;
}

bool findDay(const rotunda::Date& date, rotunda::DayEntry& out) {
  if (!date.valid()) return false;
  char path[32];
  rotunda::indexPath(date.year, date.month, path, sizeof(path));
  bool found = false;
  withReader(path, [&](rotunda::LineReader& reader, char* line, const size_t cap) {
    found = rotunda::findDay(reader, date, out, line, cap);
  });
  return found;
}

bool readMonthMarks(const int year, const int month, rotunda::MonthMarks& out) {
  out = rotunda::MonthMarks{};
  out.year = static_cast<int16_t>(year);
  out.month = static_cast<uint8_t>(month);
  char path[32];
  rotunda::indexPath(year, month, path, sizeof(path));
  return withReader(path, [&](rotunda::LineReader& reader, char* line, const size_t cap) {
    rotunda::readMonthMarks(reader, year, month, out, line, cap);
  });
}

bool scanSaid(rotunda::KeptScan& scan) {
  return withReader(SAID_PATH, [&](rotunda::LineReader& reader, char* line, const size_t cap) {
    rotunda::scanSaid(reader, scan, line, cap);
  });
}

bool readClubs(rotunda::ClubList& out) {
  out.count = 0;
  return withReader(CLUBS_PATH, [&](rotunda::LineReader& reader, char* line, const size_t cap) {
    rotunda::readClubs(reader, out, line, cap);
  });
}

bool appendKept(const rotunda::Date& officeDay, const rotunda::Office office, const bool friday) {
  bool duplicate = false;
  withReader(SAID_PATH, [&](rotunda::LineReader& reader, char* line, const size_t cap) {
    duplicate = rotunda::hasEntry(reader, officeDay, office, friday, line, cap);
  });
  if (duplicate) return true;

  const Now clock = now();
  char line[64];
  const int len = rotunda::formatSaidLine(line, sizeof(line), officeDay, office, friday, clock.wallDate,
                                          clock.hasClock ? clock.hour * 60 + clock.minute : -1);
  if (len <= 0) return false;

  Storage.ensureDirectoryExists("/office");
  HalFile file = Storage.open(SAID_PATH, O_WRONLY | O_CREAT | O_APPEND);
  if (!file) {
    LOG_ERR("ROT", "Cannot open %s for append", SAID_PATH);
    return false;
  }
  if (file.fileSize() == 0) {
    file.write(rotunda::SAID_HEADER, sizeof(rotunda::SAID_HEADER) - 1);
  }
  const size_t written = file.write(line, static_cast<size_t>(len));
  file.flush();
  if (written != static_cast<size_t>(len)) {
    LOG_ERR("ROT", "Short write to %s", SAID_PATH);
    return false;
  }
  LOG_INF("ROT", "Kept %s %s", friday ? "friday" : rotunda::officeKey(office), line);
  return true;
}

bool resolveDayEpub(const rotunda::DayEntry* entry, const rotunda::Date& date, char* out, const size_t cap) {
  if (entry && entry->epub[0] && Storage.exists(entry->epub)) {
    rotunda::copyField(out, cap, entry->epub);
    return true;
  }
  rotunda::officeEpubPath(date, out, cap);
  if (Storage.exists(out)) return true;
  out[0] = '\0';
  return false;
}

}  // namespace RotundaStore
