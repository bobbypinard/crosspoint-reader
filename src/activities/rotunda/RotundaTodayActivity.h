#pragma once

#include <OfficeIndex.h>
#include <OfficeTypes.h>

#include "RotundaScreen.h"
#include "rotunda/RotundaStore.h"

// Today's Office (screens/today.png): the day's four offices with psalms,
// lessons and kept status. The office the clock picks is focused on entry;
// OK opens it in the day's EPUB; holding OK on a Friday logs the devotion.
class RotundaTodayActivity final : public RotundaScreen {
 public:
  RotundaTodayActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : RotundaScreen("RotundaToday", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class Notice : uint8_t { None, FridayKept, NotFriday };

  void renderHeader();
  void renderOfficeRow(int index, int y, int height);
  void logFriday();

  int selected = 0;
  RotundaStore::Now now;
  bool hasDay = false;
  rotunda::DayEntry day;
  char epubPath[112] = "";
  bool epubPresent = false;
  uint8_t keptMask = 0;
  uint16_t finished[rotunda::OFFICE_COUNT] = {};
  Notice notice = Notice::None;
};
