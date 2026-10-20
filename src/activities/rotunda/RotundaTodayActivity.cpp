#include "RotundaTodayActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <OfficeHours.h>
#include <SaidLog.h>

#include <cstdio>
#include <iterator>
#include <numeric>

#include "components/themes/rotunda/RotundaChrome.h"
#include "rotunda/RotundaBooks.h"
#include "rotunda/RotundaText.h"

namespace RC = RotundaChrome;

namespace {
constexpr int HEADER_TOP = RC::TITLE_BAR_HEIGHT + 12;
constexpr int COVER_W = 120;
constexpr int COVER_H = 180;
constexpr int ROWS_TOP = HEADER_TOP + COVER_H + 18;
constexpr int OFFICE_ROW_H = 104;
// Nominal minutes per office (Rite I, said): 20 + 10 + 18 + 10 = 58.
constexpr int OFFICE_MINUTES[rotunda::OFFICE_COUNT] = {20, 10, 18, 10};
// The index carries only the MP/EP readings; Noonday and Compline use the
// fixed Prayer Book psalms.
constexpr const char* NOONDAY_PSALMS =
    "Ps 119:105\xE2\x80\x93"
    "112, 126";
constexpr const char* COMPLINE_PSALMS =
    "Ps 4, 31:1\xE2\x80\x93"
    "5, 91, 134";
constexpr int FRIDAY = 5;
}  // namespace

void RotundaTodayActivity::onEnter() {
  RotundaScreen::onEnter();
  RotundaStore::reloadHours();
  now = RotundaStore::now();
  hasDay = now.officeDay.valid() && RotundaStore::findDay(now.officeDay, day);
  epubPresent = RotundaStore::resolveDayEpub(hasDay ? &day : nullptr, now.officeDay, epubPath, sizeof(epubPath));
  if (!epubPresent) {
    // Keep the expected name for the message.
    if (hasDay && day.epub[0]) {
      rotunda::copyField(epubPath, sizeof(epubPath), day.epub);
    } else if (now.officeDay.valid()) {
      rotunda::officeEpubPath(now.officeDay, epubPath, sizeof(epubPath));
    }
  }

  rotunda::KeptScan scan;
  scan.today = now.officeDay;
  scan.year = now.officeDay.year;
  scan.month = now.officeDay.month;
  if (now.officeDay.valid()) RotundaStore::scanSaid(scan);
  keptMask = scan.todayMask;
  for (int i = 0; i < rotunda::OFFICE_COUNT; ++i) finished[i] = scan.todayFinished[i];

  selected = static_cast<int>(now.current.office);
  requestUpdate();
}

void RotundaTodayActivity::loop() {
  buttonNavigator.onNext([this] {
    selected = ButtonNavigator::nextIndex(selected, rotunda::OFFICE_COUNT);
    notice = Notice::None;
    requestUpdate();
  });
  buttonNavigator.onPrevious([this] {
    selected = ButtonNavigator::previousIndex(selected, rotunda::OFFICE_COUNT);
    notice = Notice::None;
    requestUpdate();
  });

  if (confirmLongPressed()) {
    logFriday();
    return;
  }
  if (confirmReleased()) {
    if (epubPresent) RotundaBooks::openOffice(epubPath, static_cast<rotunda::Office>(selected));
    return;
  }
  if (backReleased()) activityManager.goHome();
}

void RotundaTodayActivity::logFriday() {
  if (!now.officeDay.valid() || rotunda::weekday(now.officeDay) != FRIDAY) {
    notice = Notice::NotFriday;
  } else if (RotundaStore::appendKept(now.officeDay, rotunda::Office::Morning, true)) {
    notice = Notice::FridayKept;
  }
  requestUpdate();
}

void RotundaTodayActivity::render(RenderLock&&) {
  renderer.clearScreen();
  RC::TitleBar bar;
  bar.title = tr(STR_ROT_TODAYS_OFFICE);
  RC::drawTitleBar(renderer, bar);
  renderHeader();
  for (int i = 0; i < rotunda::OFFICE_COUNT; ++i) renderOfficeRow(i, ROWS_TOP + i * OFFICE_ROW_H, OFFICE_ROW_H);

  const int w = renderer.getScreenWidth();
  int y = ROWS_TOP + rotunda::OFFICE_COUNT * OFFICE_ROW_H + 12;
  const int textW = w - 2 * RC::ROW_TEXT_X;
  const auto drawNote = [&](const char* text) {
    for (const auto& line : renderer.wrappedText(RC::FONT_SM, text, textW, 3)) {
      renderer.drawText(RC::FONT_SM, RC::ROW_TEXT_X, y, line.c_str());
      y += renderer.getLineHeight(RC::FONT_SM);
    }
  };
  if (notice == Notice::FridayKept) drawNote(tr(STR_ROT_FRIDAY_KEPT));
  if (notice == Notice::NotFriday) drawNote(tr(STR_ROT_NOT_FRIDAY));
  if (!epubPresent) {
    drawNote(tr(STR_ROT_EPUB_MISSING));
    char expected[160];
    snprintf(expected, sizeof(expected), tr(STR_ROT_EPUB_EXPECTED), epubPath);
    drawNote(expected);
  } else if (!hasDay) {
    drawNote(tr(STR_ROT_NO_INDEX));
  }
  if (!now.hasClock) drawNote(tr(STR_ROT_NO_CLOCK));
  present();
}

void RotundaTodayActivity::renderHeader() {
  const int w = renderer.getScreenWidth();
  const Rect cover{RC::ROW_TEXT_X, HEADER_TOP, COVER_W, COVER_H};
  RC::drawCover(renderer, epubPresent ? epubPath : nullptr, cover, true, tr(STR_ROT_THE_DAILY_OFFICE));

  const int x = cover.x + COVER_W + 20;
  const int textW = w - x - RC::ROW_TEXT_X;
  int y = HEADER_TOP + 6;
  renderer.drawText(RC::FONT_ROW, x, y, tr(STR_ROT_THE_DAILY_OFFICE));
  y += renderer.getLineHeight(RC::FONT_ROW) + 2;
  const int lh = renderer.getLineHeight(RC::FONT_SM) + 4;
  char line[96];
  if (now.officeDay.valid()) {
    RotundaText::formatLongDate(now.officeDay, line, sizeof(line));
    RC::drawTextFit(renderer, RC::FONT_SM, x, y, textW, line);
  }
  y += lh;
  if (hasDay) {
    char week[48];
    snprintf(week, sizeof(week), tr(STR_ROT_WEEK_OF), day.proper);
    snprintf(line, sizeof(line), "%s \xC2\xB7 %s", week, day.year);
    RC::drawTextFit(renderer, RC::FONT_SM, x, y, textW, line);
    y += lh;
    const char* mark = day.feast[0] ? day.feast : day.commemoration;
    RC::drawTextFit(renderer, RC::FONT_SM, x, y, textW, mark);
  }

  const int total = std::accumulate(std::begin(OFFICE_MINUTES), std::end(OFFICE_MINUTES), 0);
  snprintf(line, sizeof(line), tr(STR_ROT_OFFICES_MINUTES), rotunda::OFFICE_COUNT, total);
  const int pipsY = HEADER_TOP + COVER_H - 12;
  renderer.drawText(RC::FONT_SM, x, pipsY - lh - 4, line);
  RC::drawPips(renderer, x, pipsY, keptMask, true);
  renderer.drawLine(0, ROWS_TOP - 2, w, ROWS_TOP - 2, true);
}

void RotundaTodayActivity::renderOfficeRow(const int index, const int y, const int height) {
  const int w = renderer.getScreenWidth();
  const bool selectedRow = index == selected;
  const bool ink = !selectedRow;
  const auto office = static_cast<rotunda::Office>(index);
  renderer.fillRect(0, y, w, height - 1, selectedRow);
  renderer.drawLine(0, y + height - 1, w, y + height - 1, true);

  const int right = w - RC::ROW_TEXT_X;
  char number[4];
  snprintf(number, sizeof(number), "%d", index + 1);
  renderer.drawText(RC::FONT_ROW, RC::ROW_TEXT_X, y + 12, number, ink);
  const int x = RC::ROW_TEXT_X + 36;

  char hour[16];
  rotunda::formatHour12Suffix(RotundaStore::hours().start[index], hour, sizeof(hour));
  RC::drawTextRight(renderer, RC::FONT_SM, right, y + 16, hour, ink);
  const int nameW = right - x - renderer.getTextWidth(RC::FONT_SM, hour) - 12;
  RC::drawTextFit(renderer, RC::FONT_ROW, x, y + 12, nameW, RotundaText::officeName(office), ink);

  char psalms[72] = "";
  char lessons[96] = "";
  switch (office) {
    case rotunda::Office::Morning:
      if (hasDay) {
        rotunda::formatPsalms(day.mpPsalms, "Ps ", psalms, sizeof(psalms));
        rotunda::joinLessons(day.mpLessons, lessons, sizeof(lessons));
      }
      break;
    case rotunda::Office::Noonday:
      rotunda::copyField(psalms, sizeof(psalms), NOONDAY_PSALMS);
      rotunda::copyField(lessons, sizeof(lessons), tr(STR_ROT_SHORT_LESSON));
      break;
    case rotunda::Office::Evening:
      if (hasDay) {
        rotunda::formatPsalms(day.epPsalms, "Ps ", psalms, sizeof(psalms));
        rotunda::joinLessons(day.epLessons, lessons, sizeof(lessons));
      }
      break;
    case rotunda::Office::Compline:
    case rotunda::Office::Count:
      rotunda::copyField(psalms, sizeof(psalms), COMPLINE_PSALMS);
      rotunda::copyField(lessons, sizeof(lessons), tr(STR_ROT_NO_LESSON));
      break;
  }

  // Status column: kept time, "now", or the office's length.
  char status[24];
  const bool kept = keptMask & (1u << index);
  const bool current = !kept && office == now.current.office;
  bool boldStatus = false;
  if (kept) {
    char t[8] = "";
    if (finished[index] != 0xFFFF) rotunda::formatHour12(finished[index], t, sizeof(t));
    snprintf(status, sizeof(status), tr(STR_ROT_KEPT_AT), t);
  } else if (current) {
    rotunda::copyField(status, sizeof(status), tr(STR_ROT_NOW));
    boldStatus = true;
  } else {
    snprintf(status, sizeof(status), tr(STR_ROT_MINUTES), OFFICE_MINUTES[index]);
  }
  RC::drawTextRight(renderer, RC::FONT_SM, right, y + 44, status, ink, boldStatus);
  const int statusW =
      renderer.getTextWidth(RC::FONT_SM, status, boldStatus ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
  if (kept) RC::drawCheck(renderer, right - 18, y + 72, 16, ink);
  if (current) RC::drawTriangle(renderer, right - 10, y + 72, 10, 14, false, ink);

  const int textW = right - x - statusW - 12;
  RC::drawTextFit(renderer, RC::FONT_SM, x, y + 44, textW, psalms, ink);
  RC::drawTextFit(renderer, RC::FONT_SM, x, y + 68, right - x - 28, lessons, ink);
}
