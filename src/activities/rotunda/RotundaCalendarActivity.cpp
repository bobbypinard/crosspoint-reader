#include "RotundaCalendarActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>

#include "RotundaLectionaryActivity.h"
#include "components/themes/rotunda/RotundaChrome.h"
#include "rotunda/RotundaText.h"

namespace RC = RotundaChrome;

namespace {
constexpr int GRID_X = 12;
constexpr int CELL_W = 72;
constexpr int CELL_H = 74;
constexpr int LETTERS_TOP = RC::TITLE_BAR_HEIGHT + 8;
constexpr int GRID_TOP = 92;
constexpr int STAT_ROW_H = 38;
}  // namespace

void RotundaCalendarActivity::onEnter() {
  RotundaScreen::onEnter();
  RotundaStore::reloadHours();
  now = RotundaStore::now();
  selected = now.officeDay.valid() ? now.officeDay : rotunda::Date{2026, 1, 1};
  loadMonth();
  requestUpdate();
}

void RotundaCalendarActivity::loadMonth() {
  shownYear = selected.year;
  shownMonth = selected.month;
  RotundaStore::readMonthMarks(shownYear, shownMonth, marks);
  scan = rotunda::KeptScan{};
  scan.today = now.officeDay;
  scan.year = shownYear;
  scan.month = shownMonth;
  RotundaStore::scanSaid(scan);
}

void RotundaCalendarActivity::stepDay(const int delta) {
  selected = rotunda::addDays(selected, delta);
  if (selected.year != shownYear || selected.month != shownMonth) loadMonth();
  requestUpdate();
}

void RotundaCalendarActivity::stepMonth(const int delta) {
  int y = selected.year;
  int m = selected.month + delta;
  if (m < 1) {
    m = 12;
    --y;
  } else if (m > 12) {
    m = 1;
    ++y;
  }
  selected.year = static_cast<int16_t>(y);
  selected.month = static_cast<uint8_t>(m);
  const int last = rotunda::daysInMonth(y, m);
  if (selected.day > last) selected.day = static_cast<uint8_t>(last);
  loadMonth();
  requestUpdate();
}

void RotundaCalendarActivity::loop() {
  // Front ◀/▶: release steps a day, a hold steps months. onRelease must run
  // before onContinuous so a hold's release does not also step a day.
  buttonNavigator.onRelease({MappedInputManager::Button::Right}, [this] { stepDay(1); });
  buttonNavigator.onRelease({MappedInputManager::Button::Left}, [this] { stepDay(-1); });
  buttonNavigator.onContinuous({MappedInputManager::Button::Right}, [this] { stepMonth(1); });
  buttonNavigator.onContinuous({MappedInputManager::Button::Left}, [this] { stepMonth(-1); });
  // Side keys step whole weeks.
  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Down}, [this] { stepDay(7); });
  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Up}, [this] { stepDay(-7); });

  if (confirmReleased()) {
    auto lectionary = makeUniqueNoThrow<RotundaLectionaryActivity>(renderer, mappedInput, selected, true);
    if (!lectionary) {
      LOG_ERR("ROT", "OOM: lectionary");
      return;
    }
    startActivityForResult(std::move(lectionary), [this](const ActivityResult&) {
      markFullRefresh();
      requestUpdate();
    });
    return;
  }
  if (backReleased()) activityManager.goHome();
}

void RotundaCalendarActivity::render(RenderLock&&) {
  renderer.clearScreen();
  char title[32];
  snprintf(title, sizeof(title), "%s %d", RotundaText::monthName(shownMonth), shownYear);
  RC::TitleBar bar;
  bar.title = title;
  bar.left = RC::Glyph::Back;
  bar.right = RC::Glyph::Forward;
  bar.showClock = false;
  RC::drawTitleBar(renderer, bar);

  for (int i = 0; i < 7; ++i) {
    // First letter of the short weekday name (one UTF-8 sequence).
    const char* name = RotundaText::weekdayName(i, true);
    char letter[5] = "";
    int n = 1;
    while (name[n] && (static_cast<unsigned char>(name[n]) & 0xC0) == 0x80 && n < 4) ++n;
    snprintf(letter, sizeof(letter), "%.*s", n, name);
    const int cx = GRID_X + i * CELL_W + CELL_W / 2;
    renderer.drawText(RC::FONT_SM, cx - renderer.getTextWidth(RC::FONT_SM, letter, EpdFontFamily::BOLD) / 2,
                      LETTERS_TOP, letter, true, EpdFontFamily::BOLD);
  }
  const int w = renderer.getScreenWidth();
  renderer.drawLine(GRID_X, GRID_TOP - 8, w - GRID_X, GRID_TOP - 8, true);
  renderGrid(GRID_TOP);
  present();
}

void RotundaCalendarActivity::renderGrid(const int top) {
  rotunda::Date first;
  first.year = shownYear;
  first.month = shownMonth;
  first.day = 1;
  const int lead = rotunda::weekday(first);
  const int days = rotunda::daysInMonth(shownYear, shownMonth);
  const int rows = (lead + days + 6) / 7;
  const int32_t todayNumber = now.officeDay.valid() ? rotunda::dayNumber(now.officeDay) : INT32_MAX;

  for (int d = 1; d <= days; ++d) {
    const int slot = lead + d - 1;
    const int x = GRID_X + (slot % 7) * CELL_W;
    const int y = top + (slot / 7) * CELL_H;
    rotunda::Date date = first;
    date.day = static_cast<uint8_t>(d);
    const bool isSelected = date == selected;
    const bool isToday = date == now.officeDay;
    const bool ink = !isSelected;
    if (isSelected) renderer.fillRect(x, y, CELL_W, CELL_H - 2, true);
    if (isToday && !isSelected) renderer.drawRect(x + 1, y + 1, CELL_W - 2, CELL_H - 4, 2, true);

    char number[4];
    snprintf(number, sizeof(number), "%d", d);
    const int nw = renderer.getTextWidth(RC::FONT_ROW, number);
    const int nx = x + (CELL_W - nw) / 2;
    renderer.drawText(RC::FONT_ROW, nx, y + 8, number, ink);

    const rotunda::Rank rank = marks.hasRow(d) ? marks.rank[d - 1] : rotunda::Rank::Ferial;
    if (rank == rotunda::Rank::Feast) RC::drawStar(renderer, nx + nw + 9, y + 12, 7, ink);
    if (rank == rotunda::Rank::Fast) RC::drawFish(renderer, nx + nw + 3, y + 6, 18, 11, ink);

    // Pips only for days that have happened.
    if (rotunda::dayNumber(date) <= todayNumber) {
      RC::drawPips(renderer, x + (CELL_W - RC::PIPS_WIDTH) / 2, y + 44, scan.monthMask[d - 1], ink);
    }
  }
  renderTotals(top + rows * CELL_H + 4);
}

int RotundaCalendarActivity::renderTotals(int top) {
  const int w = renderer.getScreenWidth();
  const int left = RC::ROW_TEXT_X;
  const int right = w - RC::ROW_TEXT_X;
  renderer.fillRect(GRID_X, top, w - 2 * GRID_X, 2, true);
  top += 12;

  // "So far" runs to today in the current month, the whole month in the past.
  int throughDay = 0;
  if (now.officeDay.valid()) {
    const int shown = shownYear * 12 + shownMonth;
    const int current = now.officeDay.year * 12 + now.officeDay.month;
    throughDay = shown < current    ? rotunda::daysInMonth(shownYear, shownMonth)
                 : shown == current ? now.officeDay.day
                                    : 0;
  }
  char line[48];
  snprintf(line, sizeof(line), tr(STR_ROT_SO_FAR), RotundaText::monthName(shownMonth));
  renderer.drawText(RC::FONT_ROW, left, top, line);
  top += renderer.getLineHeight(RC::FONT_ROW) + 4;

  int fridays = 0;
  int fridaysKept = 0;
  for (int d = 1; d <= throughDay; ++d) {
    rotunda::Date date{shownYear, shownMonth, static_cast<uint8_t>(d)};
    const bool fast = marks.hasRow(d) ? marks.rank[d - 1] == rotunda::Rank::Fast : rotunda::weekday(date) == 5;
    if (!fast) continue;
    ++fridays;
    if (scan.fridayKept(d)) ++fridaysKept;
  }
  const int streak = rotunda::streak(scan);

  char kept[24];
  char full[12];
  char streakText[24];
  char fridayText[24];
  snprintf(kept, sizeof(kept), tr(STR_ROT_N_OF_M), rotunda::officesKept(scan, throughDay),
           throughDay * rotunda::OFFICE_COUNT);
  snprintf(full, sizeof(full), "%d", rotunda::fullDays(scan, throughDay));
  if (streak == 1) {
    rotunda::copyField(streakText, sizeof(streakText), tr(STR_ROT_ONE_DAY));
  } else {
    snprintf(streakText, sizeof(streakText), tr(STR_ROT_DAYS), streak);
  }
  snprintf(fridayText, sizeof(fridayText), tr(STR_ROT_N_OF_M), fridaysKept, fridays);

  const struct {
    const char* label;
    const char* value;
  } rows[4] = {{tr(STR_ROT_OFFICES_KEPT), kept},
               {tr(STR_ROT_FULL_DAYS), full},
               {tr(STR_ROT_CURRENT_STREAK), streakText},
               {tr(STR_ROT_FRIDAYS_KEPT), fridayText}};
  for (const auto& row : rows) {
    const int ty = RC::centeredTextY(renderer, RC::FONT_SM, top, STAT_ROW_H);
    renderer.drawText(RC::FONT_SM, left, ty, row.label);
    RC::drawTextRight(renderer, RC::FONT_SM, right, ty, row.value, true, true);
    top += STAT_ROW_H;
    renderer.drawLine(left, top - 1, right, top - 1, true);
  }

  // Legend.
  top += 10;
  int x = left;
  RC::drawStar(renderer, x + 6, top + 9, 6, true);
  x += 16;
  renderer.drawText(RC::FONT_XS, x, top, tr(STR_ROT_LEGEND_FEAST));
  x += renderer.getTextWidth(RC::FONT_XS, tr(STR_ROT_LEGEND_FEAST)) + 14;
  renderer.drawText(RC::FONT_XS, x, top, tr(STR_ROT_LEGEND_FISH));
  x += renderer.getTextWidth(RC::FONT_XS, tr(STR_ROT_LEGEND_FISH)) + 14;
  renderer.fillRect(x, top + 5, 9, 9, true);
  renderer.drawText(RC::FONT_XS, x + 14, top, tr(STR_ROT_LEGEND_KEPT));
  return top + renderer.getLineHeight(RC::FONT_XS);
}
