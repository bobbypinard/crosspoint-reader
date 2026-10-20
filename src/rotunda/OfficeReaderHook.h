#pragma once

#include <OfficeToc.h>
#include <OfficeTypes.h>

#include <cstdint>
#include <memory>

class Epub;
class GfxRenderer;
class Section;

// Reader-side Rotunda behaviour for Daily Office EPUBs: marks an office kept
// when its last page is shown (DATA.md said.tsv) and, with the Rotunda theme,
// draws the passage title bar and minutes-left scrubber (screens/reader.png).
// Created only for Daily Office EPUBs, so other books carry no state.
class OfficeReaderHook {
 public:
  // Null unless the book is a Daily Office EPUB. Reads the TOC once.
  static std::unique_ptr<OfficeReaderHook> create(const Epub& epub);

  // Rotunda chrome replaces the status bar: Rotunda theme and portrait only.
  static bool chromeActive(const GfxRenderer& renderer);
  static constexpr int TOP_INSET = 46;
  static constexpr int BOTTOM_INSET = 64;

  // Once per page change, after the page is on screen. spine is the section's spine index.
  void onPageShown(const Epub& epub, const Section& section, int spine);
  void drawChrome(const GfxRenderer& renderer, const Epub& epub, const Section& section, int spine);

 private:
  void refreshBounds(const Epub& epub, const Section& section, int spine);
  int minutesFor(const Epub& epub, const Section& section, int spine, int office, bool before) const;

  rotunda::Date bookDate;
  rotunda::OfficeToc toc;
  int boundsPages = -1;
  uint8_t loggedMask = 0;

  int chromeToc = -2;
  char chromeTitle[48] = "";
  int chromeIndex = 0;
  int chromeTotal = 0;
};
