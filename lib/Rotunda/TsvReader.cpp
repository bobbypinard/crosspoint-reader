#include "TsvReader.h"

namespace rotunda {

bool LineReader::fill() {
  if (eof) return false;
  len = source.read(chunk, chunkCap);
  pos = 0;
  if (len == 0) eof = true;
  return len > 0;
}

bool LineReader::next(char* line, const size_t cap) {
  if (cap == 0) return false;
  size_t out = 0;
  bool any = false;
  while (true) {
    if (pos >= len && !fill()) break;
    any = true;
    const char c = chunk[pos++];
    if (c == '\n') break;
    if (c == '\r') continue;
    if (out + 1 < cap) line[out++] = c;
  }
  line[out] = '\0';
  return any;
}

int splitTsv(char* line, char** fields, const int maxFields) {
  if (maxFields <= 0) return 0;
  int count = 0;
  fields[count++] = line;
  for (char* p = line; *p && count < maxFields; ++p) {
    if (*p == '\t') {
      *p = '\0';
      fields[count++] = p + 1;
    }
  }
  return count;
}

}  // namespace rotunda
