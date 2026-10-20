#pragma once

#include <OfficeIndex.h>
#include <OfficeTypes.h>

#include <string>

#include "RotundaScreen.h"
#include "rotunda/RotundaStore.h"

// Rotunda Home (screens/home.png): seven-row menu, an art pane that follows
// the highlighted row, and the "Continue reading" strip.
class RotundaHomeActivity final : public RotundaScreen {
 public:
  enum Row : uint8_t {
    ROW_TODAY = 0,
    ROW_CALENDAR,
    ROW_LECTIONARY,
    ROW_LIBRARY,
    ROW_CLUBS,
    ROW_SHUFFLE,
    ROW_SETTINGS,
    ROW_COUNT
  };

  RotundaHomeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, Row initialRow = ROW_TODAY)
      : RotundaScreen("RotundaHome", renderer, mappedInput), selected(initialRow) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool isHomeActivity() const override { return true; }

 private:
  void activate();
  void renderMenu();
  void renderContinueStrip();
  void renderArtPane();
  void renderTodayPane(int x, int width);
  void renderCalendarPane(int x, int width);
  void renderLectionaryPane(int x, int width);
  void renderLibraryPane(int x, int width);
  void renderClubsPane(int x, int width);
  void renderTextPane(int x, int width, const char* heading, const char* body);
  void shufflePsalms();

  int selected;
  RotundaStore::Now now;
  bool hasDay = false;
  rotunda::DayEntry day;
  char dayEpub[112] = "";
  uint8_t keptMask = 0;
  int monthKept = 0;
  int monthPossible = 0;
  int streakDays = 0;

  // Continue reading: the most recent book that is not a Daily Office EPUB.
  std::string continuePath;
  std::string continueTitle;
  int continueChapter = 0;
  int continuePercent = 0;

  // Next club meeting.
  char clubName[48] = "";
  rotunda::Date clubMeets;
  bool showNoPsalter = false;
};
