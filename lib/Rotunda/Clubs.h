#pragma once

#include <cstdint>

#include "OfficeTypes.h"
#include "TsvReader.h"

namespace rotunda {

// One row of /clubs/clubs.tsv.
struct Club {
  uint8_t slot = 0;  // 1-4
  char name[48] = "";
  char book[128] = "";
  char assignment[48] = "";
  int16_t startToc = 0;  // 1-based; 0 = none
  int16_t endToc = 0;
  Date meets;
  int16_t meetsMinutes = -1;  // minutes after midnight, -1 = no time
  char notes[128] = "";
};

struct ClubList {
  static constexpr int MAX_CLUBS = 4;
  Club clubs[MAX_CLUBS];
  uint8_t count = 0;  // rows read, sorted by slot
};

// Rows with a slot outside 1-4 or a duplicate slot are skipped.
void readClubs(LineReader& reader, ClubList& out, char* line, size_t lineCap);

}  // namespace rotunda
