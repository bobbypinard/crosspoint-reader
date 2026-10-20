#pragma once

#include <cstdint>

#include "components/themes/BaseTheme.h"
#include "fontIds.h"

class GfxRenderer;

// Drawing primitives for the Rotunda design (docs/rotunda/DESIGN.md "Shared
// chrome"). Everything here is pure black/white so moving it stays on the fast
// 1-bit refresh; the title-bar grey is an ordered dither, not a grey ink.
namespace RotundaChrome {

// Type tokens.
constexpr int FONT_XS = ROTUNDA_XS_FONT_ID;        // 14 px 500 (bold: 700)
constexpr int FONT_SM = ROTUNDA_SM_FONT_ID;        // 17 px 500 (bold: 700)
constexpr int FONT_ROW = ROTUNDA_ROW_FONT_ID;      // 23 px 600
constexpr int FONT_TITLE = ROTUNDA_TITLE_FONT_ID;  // 28 px 700

constexpr int TITLE_BAR_HEIGHT = 46;  // includes the 2 px rule
constexpr int ROW_HEIGHT = 56;
constexpr int ROW_TEXT_X = 16;
constexpr int SPLIT_X = 286;  // Home menu column width; 2 px divider follows

enum class Glyph : uint8_t { None, Play, Lock, Back, Forward };

struct TitleBar {
  const char* title = nullptr;
  Glyph left = Glyph::Play;
  Glyph leftSecond = Glyph::None;    // the sleep screen shows ▶ then the lock
  Glyph right = Glyph::None;         // drawn left of the battery (Calendar ">")
  const char* rightLabel = nullptr;  // replaces the clock ("2 of 4")
  bool showClock = true;
  bool showBattery = true;
  // Stock screens pass the UI font: their titles can be Hebrew/Arabic, which Inter lacks.
  int titleFont = FONT_ROW;
  bool titleBold = false;
};

void drawTitleBar(const GfxRenderer& renderer, const TitleBar& bar);

// 56 px list row: label at x = 16, chevron at right - 22, black when selected.
void drawRow(const GfxRenderer& renderer, int x, int y, int width, int height, const char* label, bool selected,
             bool chevron = true, int fontId = FONT_ROW);
void drawChevron(const GfxRenderer& renderer, int cx, int cy, bool black, bool pointsLeft = false);

// Four 7 px squares, 4 px apart (40 px wide); the first keptCount are filled.
constexpr int PIPS_WIDTH = 4 * 7 + 3 * 4;
// Pip i reflects office i (bit i of keptMask), not a left-packed count, so a
// Compline-only day marks the last pip rather than the first.
void drawPips(const GfxRenderer& renderer, int x, int y, uint8_t keptMask, bool black);

void drawStar(const GfxRenderer& renderer, int cx, int cy, int radius, bool black);
void drawFish(const GfxRenderer& renderer, int x, int y, int width, int height, bool black);
void drawCheck(const GfxRenderer& renderer, int x, int y, int size, bool black);
// Solid triangle; pointsLeft draws ◀ (due), otherwise ▶ (play).
void drawTriangle(const GfxRenderer& renderer, int x, int y, int width, int height, bool pointsLeft, bool black);
void drawLockGlyph(const GfxRenderer& renderer, int x, int y, bool black);
void drawBattery(const GfxRenderer& renderer, int x, int y, bool black);

// Thin progress bar: 1 px outline, black fill.
void drawBar(const GfxRenderer& renderer, int x, int y, int width, int height, int permille, bool black);
// Reader scrubber: 2 px outline, fill and a diamond marker at the fill edge.
void drawScrubber(const GfxRenderer& renderer, int x, int y, int width, int height, int permille);

// Ordered (Bayer 4x4) dither of one ink density over a rectangle.
// density is 0..16 (16 = solid black).
void fillDither(const GfxRenderer& renderer, int x, int y, int width, int height, int density);

// Draws text right-aligned so it ends at rightX.
void drawTextRight(const GfxRenderer& renderer, int fontId, int rightX, int y, const char* text, bool black = true,
                   bool bold = false);
// Draws text truncated with an ellipsis to fit maxWidth.
void drawTextFit(const GfxRenderer& renderer, int fontId, int x, int y, int maxWidth, const char* text,
                 bool black = true, bool bold = false);
// Vertical offset that centres a font's line box in a band of the given height.
int centeredTextY(const GfxRenderer& renderer, int fontId, int bandY, int bandHeight);

// Book cover from the 1-bit thumbnail cache, generated on first use and
// cropped to fill the slot; draws a framed placeholder with the title when
// the book has no usable cover. frame adds a 1 px black border.
void drawCover(GfxRenderer& renderer, const char* bookPath, Rect slot, bool frame, const char* fallbackTitle);

}  // namespace RotundaChrome
