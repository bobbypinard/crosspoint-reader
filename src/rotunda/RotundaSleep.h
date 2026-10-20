#pragma once

#include <string>

#include "components/themes/BaseTheme.h"

class GfxRenderer;

// "Now Praying" sleep screen (screens/lock.png): today's cover, the office due,
// the commemoration and the four-segment day scrubber.
namespace RotundaSleep {

struct Layout {
  std::string coverPath;  // cached 2-bit cover BMP, empty when unavailable
  Rect coverSlot;
};

// Draws everything except the cover pixels (title bar, cover frame, text,
// scrubber) into the framebuffer in black and white.
Layout drawFrame(GfxRenderer& renderer);

// Grey cover for the sleep screen: the day's EPUB cover converted once to a
// 2-bit BMP fitted to 300 x 450 and cached as rotunda_lock.bmp in the book's cache.
std::string lockCoverPath(const std::string& epubPath);

}  // namespace RotundaSleep
