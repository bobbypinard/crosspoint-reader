#include "CoverFlowCache.h"

#include <Epub.h>
#include <FsHelpers.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace CoverFlowCache {

namespace {
// Thumbs are at most 0.6 x height wide; this height yields a 236 px wide 2:3 cover.
constexpr int CENTER_THUMB_HEIGHT = (CENTER_W * 5 + 2) / 3;

uint32_t readLe32(const uint8_t* p) {
  uint32_t v;
  memcpy(&v, p, sizeof(v));
  return v;
}

void writeLe32(uint8_t* p, const uint32_t v) { memcpy(p, &v, sizeof(v)); }

void writeLe16(uint8_t* p, const uint16_t v) { memcpy(p, &v, sizeof(v)); }

int rowStride(const int width) { return ((width + 31) / 32) * 4; }

// 1-bit source image held in RAM while one strip is generated.
struct Mono {
  int width = 0;
  int height = 0;
  int stride = 0;
  bool topDown = false;
  bool invert = false;  // palette index 0 is white
  std::unique_ptr<uint8_t[]> rows;

  // True when the pixel is white.
  bool white(const int x, const int y) const {
    const int row = topDown ? y : height - 1 - y;
    const bool bit = (rows[row * stride + x / 8] >> (7 - x % 8)) & 1;
    return invert ? !bit : bit;
  }
};

bool loadMono(const std::string& path, Mono& out) {
  HalFile file;
  if (!Storage.openFileForRead("ROT", path, file)) return false;
  uint8_t header[62];
  if (file.read(header, sizeof(header)) != static_cast<int>(sizeof(header)) || header[0] != 'B' || header[1] != 'M') {
    return false;
  }
  const uint32_t dataOffset = readLe32(header + 10);
  const auto width = static_cast<int32_t>(readLe32(header + 18));
  const auto height = static_cast<int32_t>(readLe32(header + 22));
  const uint16_t bpp = header[28] | (header[29] << 8);
  if (bpp != 1 || width <= 0 || width > 600 || height == 0 || std::abs(height) > 900) {
    LOG_ERR("ROT", "Cover flow source is not a 1-bit thumb: %s", path.c_str());
    return false;
  }
  out.width = width;
  out.height = std::abs(height);
  out.topDown = height < 0;
  out.stride = rowStride(width);
  // Palette entry 0 (bytes 54..57): white when its blue channel is set.
  out.invert = header[54] > 0x80;
  const size_t bytes = static_cast<size_t>(out.stride) * static_cast<size_t>(out.height);
  out.rows = makeUniqueNoThrow<uint8_t[]>(bytes);
  if (!out.rows) {
    LOG_ERR("ROT", "OOM: cover flow source (%u bytes)", static_cast<unsigned>(bytes));
    return false;
  }
  if (!file.seekSet(dataOffset) || file.read(out.rows.get(), bytes) != static_cast<int>(bytes)) return false;
  return true;
}

// Writes the strip as a top-down 1-bit BMP (index 0 black, 1 white), one row at a
// time, in the same layout as the firmware's thumbnail writer.
bool writeStrip(const Mono& src, const std::string& path, const bool leftSide) {
  const int w = SIDE_W;
  const int h = CENTER_H;
  const int stride = rowStride(w);
  uint8_t header[62] = {};
  header[0] = 'B';
  header[1] = 'M';
  writeLe32(header + 2, sizeof(header) + static_cast<uint32_t>(stride * h));
  writeLe32(header + 10, sizeof(header));
  writeLe32(header + 14, 40);
  writeLe32(header + 18, static_cast<uint32_t>(w));
  writeLe32(header + 22, static_cast<uint32_t>(-h));
  writeLe16(header + 26, 1);
  writeLe16(header + 28, 1);
  writeLe32(header + 46, 2);
  // Palette: 0 = black, 1 = white.
  header[58] = header[59] = header[60] = 0xFF;

  const std::string tmp = path + ".tmp";
  {
    HalFile out;
    if (!Storage.openFileForWrite("ROT", tmp, out)) return false;
    if (out.write(header, sizeof(header)) != sizeof(header)) return false;
    uint8_t row[(SIDE_W + 31) / 32 * 4];
    for (int y = 0; y < h; ++y) {
      memset(row, 0, sizeof(row));
      for (int x = 0; x < w; ++x) {
        // Distance from the outer edge, 0..1.
        const int fromOuter = leftSide ? x : w - 1 - x;
        const int colH = SIDE_OUTER_H + (CENTER_H - SIDE_OUTER_H) * fromOuter / (w - 1);
        const int top = (CENTER_H - colH) / 2;
        if (y < top || y >= top + colH) continue;  // background stays black
        const int sx = x * src.width / w;
        const int sy = (y - top) * src.height / colH;
        if (src.white(sx, sy)) row[x / 8] |= static_cast<uint8_t>(0x80 >> (x % 8));
      }
      if (out.write(row, static_cast<size_t>(stride)) != static_cast<size_t>(stride)) return false;
    }
  }
  Storage.remove(path.c_str());
  return Storage.rename(tmp.c_str(), path.c_str());
}

std::string generateCenter(const std::string& bookPath) {
  if (!FsHelpers::hasReflowableBookExtension(bookPath) || !Storage.exists(bookPath.c_str())) return {};
  auto epub = makeUniqueNoThrow<Epub>(bookPath, "/.crosspoint");
  if (!epub) {
    LOG_ERR("ROT", "OOM: cover flow EPUB");
    return {};
  }
  if (!epub->generateThumbBmpFromSource(CENTER_THUMB_HEIGHT)) return {};
  return epub->getThumbBmpPath(CENTER_THUMB_HEIGHT);
}
}  // namespace

std::string centerPath(const std::string& bookPath) { return generateCenter(bookPath); }

std::string sidePath(const std::string& bookPath, const bool leftSide) {
  const std::string center = generateCenter(bookPath);
  const size_t slash = center.find_last_of('/');
  if (slash == std::string::npos) return {};
  // Strips live beside the thumbnail in the book's cache directory.
  const std::string path = center.substr(0, slash) + (leftSide ? "/flow_l.bmp" : "/flow_r.bmp");
  if (Storage.exists(path.c_str())) return path;

  Mono src;
  if (!loadMono(center, src)) return {};
  if (!writeStrip(src, path, leftSide)) {
    LOG_ERR("ROT", "Cannot write %s", path.c_str());
    Storage.remove((path + ".tmp").c_str());
    return {};
  }
  return path;
}

}  // namespace CoverFlowCache
