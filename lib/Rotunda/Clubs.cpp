#include "Clubs.h"

#include <cstdlib>
#include <cstring>

namespace rotunda {

namespace {
int16_t parseToc(const char* s) {
  if (!s || !*s) return 0;
  const long v = strtol(s, nullptr, 10);
  return v > 0 && v < 10000 ? static_cast<int16_t>(v) : 0;
}
}  // namespace

void readClubs(LineReader& reader, ClubList& out, char* line, const size_t lineCap) {
  out.count = 0;
  bool header = true;
  while (reader.next(line, lineCap)) {
    if (header) {
      header = false;
      if (strncmp(line, "slot", 4) == 0) continue;
    }
    char* f[8];
    const int n = splitTsv(line, f, 8);
    if (n < 3) continue;
    char empty[1] = {'\0'};
    for (int i = n; i < 8; ++i) f[i] = empty;
    const long slot = strtol(f[0], nullptr, 10);
    if (slot < 1 || slot > ClubList::MAX_CLUBS) continue;
    bool duplicate = false;
    for (int i = 0; i < out.count; ++i) duplicate |= out.clubs[i].slot == slot;
    if (duplicate || out.count >= ClubList::MAX_CLUBS) continue;

    // Insert sorted by slot.
    int at = out.count;
    while (at > 0 && out.clubs[at - 1].slot > slot) {
      out.clubs[at] = out.clubs[at - 1];
      --at;
    }
    Club& c = out.clubs[at];
    c = Club{};
    c.slot = static_cast<uint8_t>(slot);
    copyField(c.name, sizeof(c.name), f[1]);
    copyField(c.book, sizeof(c.book), f[2]);
    copyField(c.assignment, sizeof(c.assignment), f[3]);
    c.startToc = parseToc(f[4]);
    c.endToc = parseToc(f[5]);
    if (parseIsoDate(f[6], c.meets)) {
      const char* t = strchr(f[6], 'T');
      uint16_t minutes = 0;
      if (t && parseClockMinutes(t + 1, minutes)) c.meetsMinutes = static_cast<int16_t>(minutes);
    }
    copyField(c.notes, sizeof(c.notes), f[7]);
    ++out.count;
  }
}

}  // namespace rotunda
