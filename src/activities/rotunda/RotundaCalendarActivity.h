#pragma once

#include <OfficeIndex.h>
#include <SaidLog.h>

#include "RotundaScreen.h"
#include "rotunda/RotundaStore.h"

// Monthly calendar (screens/calendar.png): pips per day from said.tsv, feast
// and fish marks from the index, month totals. Front ◀/▶ walk days (hold to
// change month), the side keys walk weeks; OK opens the day in the Lectionary.
class RotundaCalendarActivity final : public RotundaScreen {
 public:
  RotundaCalendarActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : RotundaScreen("RotundaCalendar", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  void loadMonth();
  void stepDay(int delta);
  void stepMonth(int delta);
  void renderGrid(int top);
  int renderTotals(int top);

  RotundaStore::Now now;
  rotunda::Date selected;
  int16_t shownYear = 0;
  uint8_t shownMonth = 0;
  rotunda::MonthMarks marks;
  rotunda::KeptScan scan;
};
