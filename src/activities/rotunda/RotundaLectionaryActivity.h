#pragma once

#include <I18n.h>
#include <OfficeIndex.h>
#include <OfficeTypes.h>

#include "RotundaScreen.h"
#include "rotunda/RotundaStore.h"

// Lectionary archive (screens/lectionary.png), read only from the index so past
// days work after their EPUBs are deleted. Front ◀/▶ walk days along the strip;
// the side keys move focus through the reading rows and the black "open" bar.
class RotundaLectionaryActivity final : public RotundaScreen {
 public:
  // pushed: opened from the Calendar, so BACK returns there instead of Home.
  RotundaLectionaryActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const rotunda::Date& day,
                            bool pushed = false)
      : RotundaScreen("RotundaLectionary", renderer, mappedInput), selected(day), pushed(pushed) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  static constexpr int MAX_ROWS = 10;
  enum class Section : uint8_t { Morning, Evening, Collect };
  struct ReadingRow {
    Section section;
    StrId label;
    char value[72];
  };

  void loadDay();
  void stepDay(int delta);
  void openFocused();
  void renderStrip(int top);
  int renderDetails(int top);

  rotunda::Date selected;
  rotunda::Date today;
  rotunda::Date stripEnd;
  bool pushed;
  bool hasDay = false;
  rotunda::DayEntry day;
  char epubPath[112] = "";
  bool epubPresent = false;
  ReadingRow rows[MAX_ROWS] = {};
  int rowCount = 0;
  int focus = 0;  // 0..rowCount-1 = rows, rowCount = the open bar
};
