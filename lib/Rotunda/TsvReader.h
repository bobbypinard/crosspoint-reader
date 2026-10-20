#pragma once

#include <cstddef>

namespace rotunda {

// Byte stream the TSV readers pull from: a HalFile on the device, a FILE* in
// host tests. Returns bytes read, 0 at end of file.
class ByteSource {
 public:
  virtual ~ByteSource() = default;
  virtual size_t read(char* buf, size_t cap) = 0;
};

// Reads one line at a time into a caller-owned buffer, so a month file is never
// held in RAM. Strips "\r\n"; an overlong line is truncated and the rest skipped.
class LineReader {
 public:
  // chunk is the read-ahead buffer (caller-owned, typically 128-256 bytes).
  LineReader(ByteSource& source, char* chunk, size_t chunkCap) : source(source), chunk(chunk), chunkCap(chunkCap) {}

  // False at end of file. line is always terminated.
  bool next(char* line, size_t cap);

 private:
  bool fill();

  ByteSource& source;
  char* chunk;
  size_t chunkCap;
  size_t pos = 0;
  size_t len = 0;
  bool eof = false;
};

// Splits a line in place on tabs. Returns the number of fields found (fields
// past maxFields are left joined in the last one).
int splitTsv(char* line, char** fields, int maxFields);

}  // namespace rotunda
