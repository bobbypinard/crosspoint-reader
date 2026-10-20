#include "RotundaChrome.h"

#include <Bitmap.h>
#include <Epub.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalClock.h>
#include <HalPowerManager.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "CrossPointSettings.h"
#include "components/HeaderBackTapTarget.h"

namespace RotundaChrome {

namespace {
constexpr uint8_t BAYER4[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
constexpr int BATTERY_W = 32;
constexpr int BATTERY_H = 15;
constexpr int EDGE = 16;
// Line boxes centre high in the bar (descender space); this drops caps to its visual middle.
constexpr int TITLE_TEXT_NUDGE = 3;

void drawGlyph(const GfxRenderer& renderer, const Glyph glyph, const int x, const int cy) {
  switch (glyph) {
    case Glyph::Play:
      drawTriangle(renderer, x, cy - 8, 11, 16, false, true);
      break;
    case Glyph::Lock:
      drawLockGlyph(renderer, x, cy - 9, true);
      break;
    case Glyph::Back:
      drawChevron(renderer, x + 4, cy, true, true);
      break;
    case Glyph::Forward:
      drawChevron(renderer, x + 4, cy, true, false);
      break;
    case Glyph::None:
      break;
  }
}
}  // namespace

void fillDither(const GfxRenderer& renderer, const int x, const int y, const int width, const int height,
                const int density) {
  if (density <= 0) return;
  if (density >= 16) {
    renderer.fillRect(x, y, width, height, true);
    return;
  }
  for (int yy = y; yy < y + height; ++yy) {
    for (int xx = x; xx < x + width; ++xx) {
      if (BAYER4[yy & 3][xx & 3] < density) renderer.drawPixel(xx, yy, true);
    }
  }
}

void drawTitleBar(const GfxRenderer& renderer, const TitleBar& bar) {
  const int w = renderer.getScreenWidth();
  constexpr int band = TITLE_BAR_HEIGHT - 2;
  renderer.fillRect(0, 0, w, band, false);
  // Vertical gradient from #AAAAAA (16/3 of 16 dither cells black) to white.
  for (int y = 0; y < band; ++y) {
    const int density = (16 * (band - 1 - y) + 3 * (band - 1) / 2) / (3 * (band - 1));
    for (int x = 0; x < w; ++x) {
      if (BAYER4[y & 3][x & 3] < density) renderer.drawPixel(x, y, true);
    }
  }
  renderer.fillRect(0, band, w, 2, true);
  HeaderBackTapTarget::clear();

  const int cy = band / 2;
  int leftEdge = EDGE;
  if (bar.left != Glyph::None) {
    drawGlyph(renderer, bar.left, leftEdge, cy);
    leftEdge += 16;
  }
  if (bar.leftSecond != Glyph::None) {
    drawGlyph(renderer, bar.leftSecond, leftEdge + 4, cy);
    leftEdge += 20;
  }

  int rightEdge = w - EDGE;
  if (bar.showBattery) {
    drawBattery(renderer, rightEdge - BATTERY_W, cy - BATTERY_H / 2, true);
    rightEdge -= BATTERY_W + 12;
  }
  if (bar.right != Glyph::None) {
    drawGlyph(renderer, bar.right, rightEdge - 12, cy);
    rightEdge -= 24;
  }
  char clock[8] = "";
  const char* label = bar.rightLabel;
  if (!label && bar.showClock && halClock.isAvailable()) {
    uint8_t h = 0;
    uint8_t m = 0;
    if (halClock.getTime(h, m)) {
      int hour = h;
      if (SETTINGS.clockFormat == 1) {
        hour %= 12;
        if (hour == 0) hour = 12;
      }
      snprintf(clock, sizeof(clock), "%d:%02d", hour, m);
      label = clock;
    }
  }
  if (label && *label) {
    const int y = centeredTextY(renderer, FONT_SM, 0, band) + TITLE_TEXT_NUDGE;
    drawTextRight(renderer, FONT_SM, rightEdge, y, label);
    rightEdge -= renderer.getTextWidth(FONT_SM, label) + 8;
  }

  if (bar.title && *bar.title) {
    // Centred on the screen, but kept clear of the side chrome.
    const int reserve = std::max(leftEdge, w - rightEdge) + 4;
    const int maxWidth = std::max(0, w - 2 * reserve);
    const auto style = bar.titleBold ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
    const std::string title = renderer.truncatedText(bar.titleFont, bar.title, maxWidth, style);
    const int tw = renderer.getTextWidth(bar.titleFont, title.c_str(), style);
    renderer.drawText(bar.titleFont, (w - tw) / 2, centeredTextY(renderer, bar.titleFont, 0, band) + TITLE_TEXT_NUDGE,
                      title.c_str(), true, style);
  }
}

int centeredTextY(const GfxRenderer& renderer, const int fontId, const int bandY, const int bandHeight) {
  return bandY + (bandHeight - renderer.getLineHeight(fontId)) / 2;
}

void drawChevron(const GfxRenderer& renderer, const int cx, const int cy, const bool black, const bool pointsLeft) {
  const int tip = pointsLeft ? cx - 4 : cx + 4;
  const int tail = pointsLeft ? cx + 3 : cx - 3;
  renderer.drawLine(tail, cy - 7, tip, cy, 3, black);
  renderer.drawLine(tip, cy, tail, cy + 7, 3, black);
}

void drawRow(const GfxRenderer& renderer, const int x, const int y, const int width, const int height,
             const char* label, const bool selected, const bool chevron, const int fontId) {
  renderer.fillRect(x, y, width, height, selected);
  const bool ink = !selected;
  const int textY = centeredTextY(renderer, fontId, y, height);
  const int maxWidth = width - ROW_TEXT_X - (chevron ? 40 : ROW_TEXT_X);
  drawTextFit(renderer, fontId, x + ROW_TEXT_X, textY, maxWidth, label, ink);
  if (chevron) drawChevron(renderer, x + width - 22, y + height / 2, ink);
}

void drawPips(const GfxRenderer& renderer, const int x, const int y, const uint8_t keptMask, const bool black) {
  for (int i = 0; i < 4; ++i) {
    const int px = x + i * 11;
    if (keptMask & (1u << i)) {
      renderer.fillRect(px, y, 7, 7, black);
    } else {
      renderer.drawRect(px, y, 7, 7, black);
    }
  }
}

void drawStar(const GfxRenderer& renderer, const int cx, const int cy, const int radius, const bool black) {
  int xs[10];
  int ys[10];
  const float inner = radius * 0.45f;
  for (int i = 0; i < 10; ++i) {
    const float r = (i % 2 == 0) ? static_cast<float>(radius) : inner;
    const float a = -1.5707963f + i * 0.6283185f;
    xs[i] = cx + static_cast<int>(std::lround(r * std::cos(a)));
    ys[i] = cy + static_cast<int>(std::lround(r * std::sin(a)));
  }
  renderer.fillPolygon(xs, ys, 10, black);
}

void drawFish(const GfxRenderer& renderer, const int x, const int y, const int width, const int height,
              const bool black) {
  // Ichthys: two arcs from the nose that cross to form the tail.
  const int cy = y + height / 2;
  const float half = height / 2.0f;
  int prevX = x;
  int prevUp = cy;
  int prevDown = cy;
  constexpr int STEPS = 12;
  for (int i = 1; i <= STEPS; ++i) {
    const float t = static_cast<float>(i) / STEPS;
    const float s = std::sin(3.1415927f * 1.25f * t);
    const int px = x + static_cast<int>(std::lround(width * t));
    const int up = cy - static_cast<int>(std::lround(half * s));
    const int down = cy + static_cast<int>(std::lround(half * s));
    renderer.drawLine(prevX, prevUp, px, up, 2, black);
    renderer.drawLine(prevX, prevDown, px, down, 2, black);
    prevX = px;
    prevUp = up;
    prevDown = down;
  }
}

void drawCheck(const GfxRenderer& renderer, const int x, const int y, const int size, const bool black) {
  const int midX = x + size / 3;
  const int bottom = y + size;
  renderer.drawLine(x, y + size / 2, midX, bottom, 2, black);
  renderer.drawLine(midX, bottom, x + size, y, 2, black);
}

void drawTriangle(const GfxRenderer& renderer, const int x, const int y, const int width, const int height,
                  const bool pointsLeft, const bool black) {
  int xs[3];
  int ys[3] = {y, y + height / 2, y + height};
  if (pointsLeft) {
    xs[0] = x + width;
    xs[1] = x;
    xs[2] = x + width;
  } else {
    xs[0] = x;
    xs[1] = x + width;
    xs[2] = x;
  }
  renderer.fillPolygon(xs, ys, 3, black);
}

void drawLockGlyph(const GfxRenderer& renderer, const int x, const int y, const bool black) {
  // Shackle over a solid body, 14 x 18.
  renderer.drawRect(x + 3, y, 8, 9, 2, black);
  renderer.fillRect(x, y + 7, 14, 11, black);
}

void drawBattery(const GfxRenderer& renderer, const int x, const int y, const bool black) {
  const int bodyW = BATTERY_W - 3;
  renderer.drawRect(x, y, bodyW, BATTERY_H, 2, black);
  renderer.fillRect(x + bodyW, y + 4, 3, BATTERY_H - 8, black);
  const int pct = std::clamp(static_cast<int>(powerManager.getBatteryPercentage()), 0, 100);
  const int inner = bodyW - 6;
  const int fill = inner * pct / 100;
  if (fill > 0) renderer.fillRect(x + 3, y + 3, fill, BATTERY_H - 6, black);
}

void drawBar(const GfxRenderer& renderer, const int x, const int y, const int width, const int height,
             const int permille, const bool black) {
  renderer.fillRect(x, y, width, height, !black);
  renderer.drawRect(x, y, width, height, black);
  const int fill = (width - 2) * std::clamp(permille, 0, 1000) / 1000;
  if (fill > 0) renderer.fillRect(x + 1, y + 1, fill, height - 2, black);
}

void drawScrubber(const GfxRenderer& renderer, const int x, const int y, const int width, const int height,
                  const int permille) {
  renderer.fillRect(x, y, width, height, false);
  renderer.drawRect(x, y, width, height, 2, true);
  const int fill = (width - 4) * std::clamp(permille, 0, 1000) / 1000;
  if (fill > 0) renderer.fillRect(x + 2, y + 2, fill, height - 4, true);
  // Diamond marker centred on the fill edge.
  const int mx = x + 2 + fill;
  const int my = y + height / 2;
  const int r = height / 2 + 4;
  int xs[4] = {mx, mx + r, mx, mx - r};
  int ys[4] = {my - r, my, my + r, my};
  renderer.fillPolygon(xs, ys, 4, true);
}

void drawTextRight(const GfxRenderer& renderer, const int fontId, const int rightX, const int y, const char* text,
                   const bool black, const bool bold) {
  if (!text || !*text) return;
  const auto style = bold ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
  renderer.drawText(fontId, rightX - renderer.getTextWidth(fontId, text, style), y, text, black, style);
}

void drawTextFit(const GfxRenderer& renderer, const int fontId, const int x, const int y, const int maxWidth,
                 const char* text, const bool black, const bool bold) {
  if (!text || !*text || maxWidth <= 0) return;
  const auto style = bold ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
  if (renderer.getTextWidth(fontId, text, style) <= maxWidth) {
    renderer.drawText(fontId, x, y, text, black, style);
    return;
  }
  const std::string fitted = renderer.truncatedText(fontId, text, maxWidth, style);
  renderer.drawText(fontId, x, y, fitted.c_str(), black, style);
}

void drawCover(GfxRenderer& renderer, const char* bookPath, const Rect slot, const bool frame,
               const char* fallbackTitle) {
  bool drawn = false;
  if (bookPath && *bookPath && FsHelpers::hasReflowableBookExtension(bookPath) && Storage.exists(bookPath)) {
    // Thumbs are generated at most 0.6 x height wide; pick the height whose
    // width fills the slot so a 2:3 cover is cropped, never letterboxed.
    const int thumbHeight = std::max(slot.height, (slot.width * 5 + 2) / 3);
    std::string thumbPath;
    {
      // EPUB objects exceed the stack budget; one lives only for this lookup.
      auto epub = makeUniqueNoThrow<Epub>(bookPath, "/.crosspoint");
      if (!epub) {
        LOG_ERR("ROT", "OOM: cover EPUB");
      } else if (epub->generateThumbBmpFromSource(thumbHeight)) {
        thumbPath = epub->getThumbBmpPath(thumbHeight);
      }
    }
    HalFile file;
    if (!thumbPath.empty() && Storage.openFileForRead("ROT", thumbPath, file)) {
      Bitmap bitmap(file);
      if (bitmap.parseHeaders() == BmpReaderError::Ok) {
        renderer.fillRect(slot.x, slot.y, slot.width, slot.height, false);
        drawn = BaseTheme::drawCoverThumbFill(renderer, bitmap, slot);
      }
    }
  }
  if (!drawn) {
    renderer.fillRect(slot.x, slot.y, slot.width, slot.height, false);
    renderer.drawRect(slot.x + 4, slot.y + 4, slot.width - 8, slot.height - 8, true);
    // Titles only fit legibly in covers at least 80 px wide.
    if (fallbackTitle && *fallbackTitle && slot.width >= 80) {
      const int font = slot.width >= 150 ? FONT_SM : FONT_XS;
      const auto lines = renderer.wrappedText(font, fallbackTitle, slot.width - 20, 4);
      const int lh = renderer.getLineHeight(font);
      int y = slot.y + (slot.height - static_cast<int>(lines.size()) * lh) / 2;
      for (const auto& line : lines) {
        const int tw = renderer.getTextWidth(font, line.c_str());
        renderer.drawText(font, slot.x + (slot.width - tw) / 2, y, line.c_str());
        y += lh;
      }
    }
  }
  if (frame) renderer.drawRect(slot.x, slot.y, slot.width, slot.height, true);
}

}  // namespace RotundaChrome
