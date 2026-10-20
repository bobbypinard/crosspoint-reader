#include "RotundaHomeActivity.h"

#include <Epub.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>
#include <esp_random.h>

#include <algorithm>
#include <cstdio>

#include "RecentBooksStore.h"
#include "RotundaCalendarActivity.h"
#include "RotundaClubsActivity.h"
#include "RotundaLectionaryActivity.h"
#include "RotundaLibraryActivity.h"
#include "RotundaTodayActivity.h"
#include "components/themes/rotunda/RotundaChrome.h"
#include "rotunda/RotundaBooks.h"
#include "rotunda/RotundaText.h"

namespace RC = RotundaChrome;

namespace {
constexpr int MENU_TOP = RC::TITLE_BAR_HEIGHT + 2;
constexpr int PANE_X = RC::SPLIT_X + 2;
constexpr int STRIP_TOP = MENU_TOP + RotundaHomeActivity::ROW_COUNT * RC::ROW_HEIGHT + 20;

const char* rowLabel(const int row) {
  switch (row) {
    case RotundaHomeActivity::ROW_TODAY:
      return tr(STR_ROT_TODAYS_OFFICE);
    case RotundaHomeActivity::ROW_CALENDAR:
      return tr(STR_ROT_CALENDAR);
    case RotundaHomeActivity::ROW_LECTIONARY:
      return tr(STR_ROT_LECTIONARY);
    case RotundaHomeActivity::ROW_LIBRARY:
      return tr(STR_LIBRARY);
    case RotundaHomeActivity::ROW_CLUBS:
      return tr(STR_ROT_BOOK_CLUBS);
    case RotundaHomeActivity::ROW_SHUFFLE:
      return tr(STR_ROT_SHUFFLE_PSALMS);
    case RotundaHomeActivity::ROW_SETTINGS:
      return tr(STR_SETTINGS_TITLE);
    default:
      return "";
  }
}

// Draws wrapped text; returns the y below the last line.
int drawWrapped(const GfxRenderer& renderer, const int font, const int x, int y, const int width, const char* text,
                const int maxLines, const bool bold = false) {
  if (!text || !*text) return y;
  const auto style = bold ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
  const int lh = renderer.getLineHeight(font);
  for (const auto& line : renderer.wrappedText(font, text, width, maxLines, style)) {
    renderer.drawText(font, x, y, line.c_str(), true, style);
    y += lh;
  }
  return y;
}
}  // namespace

void RotundaHomeActivity::onEnter() {
  RotundaScreen::onEnter();
  RotundaStore::reloadHours();
  now = RotundaStore::now();

  hasDay = now.officeDay.valid() && RotundaStore::findDay(now.officeDay, day);
  RotundaStore::resolveDayEpub(hasDay ? &day : nullptr, now.officeDay, dayEpub, sizeof(dayEpub));

  rotunda::KeptScan scan;
  scan.today = now.officeDay;
  scan.year = now.officeDay.year;
  scan.month = now.officeDay.month;
  if (now.officeDay.valid()) RotundaStore::scanSaid(scan);
  keptMask = scan.todayMask;
  monthKept = rotunda::officesKept(scan, now.officeDay.day);
  monthPossible = now.officeDay.day * rotunda::OFFICE_COUNT;
  streakDays = rotunda::streak(scan);

  // RECENT_BOOKS is most-recent first.
  for (const RecentBook& book : RECENT_BOOKS.getBooks()) {
    if (RotundaBooks::isDailyOfficeEpub(book.path) || RecentBooksStore::isMissing(book)) continue;
    continuePath = book.path;
    continueTitle = book.title.empty() ? book.path : book.title;
    RotundaBooks::Position pos;
    if (RotundaBooks::readPosition(book.path.c_str(), pos)) {
      continueChapter = pos.chapter;
      continuePercent = pos.percent;
    }
    break;
  }

  // Next club meeting on or after today.
  // ClubList (~1.7 KB) is over the stack budget; it lives only for this scan.
  auto clubs = makeUniqueNoThrow<rotunda::ClubList>();
  if (!clubs) {
    LOG_ERR("ROT", "OOM: club list (%u bytes)", static_cast<unsigned>(sizeof(rotunda::ClubList)));
  } else if (RotundaStore::readClubs(*clubs)) {
    const int32_t today = now.officeDay.valid() ? rotunda::dayNumber(now.officeDay) : 0;
    int32_t best = INT32_MAX;
    for (int i = 0; i < clubs->count; ++i) {
      const auto& c = clubs->clubs[i];
      if (!c.meets.valid()) continue;
      const int32_t n = rotunda::dayNumber(c.meets);
      if (n >= today && n < best) {
        best = n;
        rotunda::copyField(clubName, sizeof(clubName), c.name);
        clubMeets = c.meets;
      }
    }
  }

  requestUpdate();
}

void RotundaHomeActivity::loop() {
  buttonNavigator.onNext([this] {
    selected = ButtonNavigator::nextIndex(selected, ROW_COUNT);
    showNoPsalter = false;
    requestUpdate();
  });
  buttonNavigator.onPrevious([this] {
    selected = ButtonNavigator::previousIndex(selected, ROW_COUNT);
    showNoPsalter = false;
    requestUpdate();
  });

  if (confirmReleased()) {
    activate();
    return;
  }
  // BACK is otherwise unused on Home: resume the continue-reading book, as the stock Home does.
  if (backReleased() && !continuePath.empty()) {
    RotundaBooks::open(continuePath.c_str());
  }
}

void RotundaHomeActivity::activate() {
  std::unique_ptr<Activity> next;
  switch (selected) {
    case ROW_TODAY:
      next = makeUniqueNoThrow<RotundaTodayActivity>(renderer, mappedInput);
      break;
    case ROW_CALENDAR:
      next = makeUniqueNoThrow<RotundaCalendarActivity>(renderer, mappedInput);
      break;
    case ROW_LECTIONARY:
      next = makeUniqueNoThrow<RotundaLectionaryActivity>(renderer, mappedInput, now.officeDay);
      break;
    case ROW_LIBRARY:
      next = makeUniqueNoThrow<RotundaLibraryActivity>(renderer, mappedInput);
      break;
    case ROW_CLUBS:
      next = makeUniqueNoThrow<RotundaClubsActivity>(renderer, mappedInput);
      break;
    case ROW_SHUFFLE:
      shufflePsalms();
      return;
    case ROW_SETTINGS:
      activityManager.goToSettings();
      return;
    default:
      return;
  }
  if (!next) {
    LOG_ERR("ROT", "OOM: Rotunda screen %d", selected);
    return;
  }
  activityManager.replaceActivity(std::move(next));
}

void RotundaHomeActivity::shufflePsalms() {
  if (Storage.exists(RotundaStore::PSALTER_PATH)) {
    // Before the psalter's first open there is no TOC cache to count; it then opens at entry 0.
    const int count = RotundaBooks::tocCount(RotundaStore::PSALTER_PATH);
    const int index = count > 0 ? static_cast<int>(esp_random() % static_cast<uint32_t>(count)) : 0;
    RotundaBooks::openAtTocIndex(RotundaStore::PSALTER_PATH, index);
    return;
  }
  showNoPsalter = true;
  requestUpdate();
}

void RotundaHomeActivity::render(RenderLock&&) {
  renderer.clearScreen();
  RC::TitleBar bar;
  bar.title = tr(STR_ROT_TITLE);
  RC::drawTitleBar(renderer, bar);
  renderMenu();
  renderContinueStrip();
  renderer.fillRect(RC::SPLIT_X, RC::TITLE_BAR_HEIGHT, 2, renderer.getScreenHeight() - RC::TITLE_BAR_HEIGHT);
  renderArtPane();
  present();
}

void RotundaHomeActivity::renderMenu() {
  for (int i = 0; i < ROW_COUNT; ++i) {
    RC::drawRow(renderer, 0, MENU_TOP + i * RC::ROW_HEIGHT, RC::SPLIT_X, RC::ROW_HEIGHT, rowLabel(i), i == selected);
  }
}

void RotundaHomeActivity::renderContinueStrip() {
  if (continuePath.empty()) return;
  const int x = RC::ROW_TEXT_X;
  renderer.drawText(RC::FONT_XS, x, STRIP_TOP, tr(STR_ROT_CONTINUE_READING));
  const int top = STRIP_TOP + renderer.getLineHeight(RC::FONT_XS) + 6;
  RC::drawCover(renderer, continuePath.c_str(), Rect{x, top, 60, 90}, true, continueTitle.c_str());
  const int textX = x + 72;
  const int textW = RC::SPLIT_X - textX - 16;
  int y = drawWrapped(renderer, RC::FONT_SM, textX, top + 2, textW, continueTitle.c_str(), 2, true);
  if (continueChapter > 0) {
    char line[32];
    snprintf(line, sizeof(line), tr(STR_ROT_CHAPTER_PERCENT), continueChapter, continuePercent);
    renderer.drawText(RC::FONT_SM, textX, y + 2, line);
    y += renderer.getLineHeight(RC::FONT_SM) + 6;
  }
  RC::drawBar(renderer, textX, std::max(y + 2, top + 76), textW, 8, continuePercent * 10, true);
}

void RotundaHomeActivity::renderArtPane() {
  const int x = PANE_X + 12;
  const int width = renderer.getScreenWidth() - x - 12;
  switch (selected) {
    case ROW_TODAY:
      renderTodayPane(x, width);
      break;
    case ROW_CALENDAR:
      renderCalendarPane(x, width);
      break;
    case ROW_LECTIONARY:
      renderLectionaryPane(x, width);
      break;
    case ROW_LIBRARY:
      renderLibraryPane(x, width);
      break;
    case ROW_CLUBS:
      renderClubsPane(x, width);
      break;
    case ROW_SHUFFLE:
      renderTextPane(x, width, tr(STR_ROT_SHUFFLE_PSALMS),
                     showNoPsalter ? tr(STR_ROT_NO_PSALTER) : tr(STR_ROT_SHUFFLE_HINT));
      break;
    case ROW_SETTINGS:
      renderTextPane(x, width, tr(STR_SETTINGS_TITLE), tr(STR_ROT_SETTINGS_HINT));
      break;
    default:
      break;
  }
}

void RotundaHomeActivity::renderTodayPane(const int x, const int width) {
  const int coverW = std::min(width, 214);
  const Rect slot{x + (width - coverW) / 2, RC::TITLE_BAR_HEIGHT + 22, coverW, coverW * 3 / 2};
  RC::drawCover(renderer, dayEpub, slot, true, tr(STR_ROT_THE_DAILY_OFFICE));

  int y = slot.y + slot.height + 14;
  if (now.officeDay.valid()) {
    char heading[40];
    snprintf(heading, sizeof(heading), "%s %d", RotundaText::weekdayName(rotunda::weekday(now.officeDay)),
             now.officeDay.day);
    renderer.drawText(RC::FONT_ROW, x, y, heading);
    y += renderer.getLineHeight(RC::FONT_ROW);
  }
  if (hasDay) {
    char line[64];
    snprintf(line, sizeof(line), "%s \xC2\xB7 %s", day.proper, day.year);
    RC::drawTextFit(renderer, RC::FONT_SM, x, y, width, line);
  }
  y += renderer.getLineHeight(RC::FONT_SM) + 10;

  const int lh = renderer.getLineHeight(RC::FONT_SM) + 2;
  const auto& hours = RotundaStore::hours();
  for (int i = 0; i < rotunda::OFFICE_COUNT; ++i) {
    const auto office = static_cast<rotunda::Office>(i);
    const bool due = office == now.current.office;
    char hour[8];
    rotunda::formatHour12(hours.start[i], hour, sizeof(hour));
    const auto style = due ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
    renderer.drawText(RC::FONT_SM, x, y, hour, true, style);
    renderer.drawText(RC::FONT_SM, x + 54, y, RotundaText::officeShortName(office), true, style);
    const int markX = x + width - 20;
    if (keptMask & (1u << i)) {
      RC::drawCheck(renderer, markX, y + 4, 12, true);
    } else if (due) {
      RC::drawTriangle(renderer, markX + 2, y + 5, 9, 12, true, true);
    }
    y += lh;
  }

  if (hasDay && (day.feast[0] || day.commemoration[0])) {
    y += 8;
    renderer.drawLine(x, y, x + width, y, true);
    y += 8;
    drawWrapped(renderer, RC::FONT_SM, x, y, width, day.feast[0] ? day.feast : day.commemoration, 3);
  }
}

void RotundaHomeActivity::renderCalendarPane(const int x, const int width) {
  int y = RC::TITLE_BAR_HEIGHT + 24;
  if (!now.officeDay.valid()) return;
  char line[48];
  snprintf(line, sizeof(line), "%s %d", RotundaText::monthName(now.officeDay.month), now.officeDay.year);
  renderer.drawText(RC::FONT_ROW, x, y, line);
  y += renderer.getLineHeight(RC::FONT_ROW) + 12;
  renderer.drawText(RC::FONT_SM, x, y, tr(STR_ROT_OFFICES_KEPT));
  y += renderer.getLineHeight(RC::FONT_SM);
  snprintf(line, sizeof(line), tr(STR_ROT_N_OF_M), monthKept, monthPossible);
  renderer.drawText(RC::FONT_TITLE, x, y, line);
  y += renderer.getLineHeight(RC::FONT_TITLE) + 12;
  renderer.drawText(RC::FONT_SM, x, y, tr(STR_ROT_CURRENT_STREAK));
  y += renderer.getLineHeight(RC::FONT_SM);
  snprintf(line, sizeof(line), tr(STR_ROT_STREAK_DAYS), streakDays);
  RC::drawTextFit(renderer, RC::FONT_TITLE, x, y, width, line);
  y += renderer.getLineHeight(RC::FONT_TITLE) + 16;
  RC::drawPips(renderer, x, y, keptMask, true);
}

void RotundaHomeActivity::renderLectionaryPane(const int x, const int width) {
  int y = RC::TITLE_BAR_HEIGHT + 24;
  if (!hasDay) {
    renderTextPane(x, width, tr(STR_ROT_LECTIONARY), tr(STR_ROT_NO_INDEX));
    return;
  }
  char line[112];
  RotundaText::formatLongDate(day.date, line, sizeof(line));
  y = drawWrapped(renderer, RC::FONT_ROW, x, y, width, line, 2);
  y += 10;
  const struct {
    StrId heading;
    const char* psalms;
    const char* lessons;
  } parts[2] = {{StrId::STR_ROT_SECTION_MORNING, day.mpPsalms, day.mpLessons},
                {StrId::STR_ROT_SECTION_EVENING, day.epPsalms, day.epLessons}};
  for (const auto& part : parts) {
    renderer.drawText(RC::FONT_XS, x, y, I18N.get(part.heading), true, EpdFontFamily::BOLD);
    y += renderer.getLineHeight(RC::FONT_XS) + 2;
    rotunda::formatPsalms(part.psalms, "Ps ", line, sizeof(line));
    y = drawWrapped(renderer, RC::FONT_SM, x, y, width, line, 2);
    for (int i = 0; i < 3; ++i) {
      if (!rotunda::lessonAt(part.lessons, i, line, sizeof(line))) break;
      y = drawWrapped(renderer, RC::FONT_SM, x, y, width, line, 2);
    }
    y += 12;
  }
}

void RotundaHomeActivity::renderLibraryPane(const int x, const int width) {
  if (continuePath.empty()) {
    renderTextPane(x, width, tr(STR_LIBRARY), tr(STR_ROT_NO_BOOKS));
    return;
  }
  const int coverW = std::min(width, 214);
  const Rect slot{x + (width - coverW) / 2, RC::TITLE_BAR_HEIGHT + 22, coverW, coverW * 3 / 2};
  RC::drawCover(renderer, continuePath.c_str(), slot, true, continueTitle.c_str());
  drawWrapped(renderer, RC::FONT_SM, x, slot.y + slot.height + 14, width, continueTitle.c_str(), 3, true);
}

void RotundaHomeActivity::renderClubsPane(const int x, const int width) {
  if (!clubName[0]) {
    renderTextPane(x, width, tr(STR_ROT_BOOK_CLUBS), tr(STR_ROT_NO_MEETING));
    return;
  }
  int y = RC::TITLE_BAR_HEIGHT + 24;
  renderer.drawText(RC::FONT_XS, x, y, tr(STR_ROT_NEXT_MEETING), true, EpdFontFamily::BOLD);
  y += renderer.getLineHeight(RC::FONT_XS) + 4;
  y = drawWrapped(renderer, RC::FONT_ROW, x, y, width, clubName, 3);
  char date[40];
  RotundaText::formatLongDate(clubMeets, date, sizeof(date));
  drawWrapped(renderer, RC::FONT_SM, x, y + 4, width, date, 2);
}

void RotundaHomeActivity::renderTextPane(const int x, const int width, const char* heading, const char* body) {
  int y = RC::TITLE_BAR_HEIGHT + 24;
  y = drawWrapped(renderer, RC::FONT_ROW, x, y, width, heading, 2);
  drawWrapped(renderer, RC::FONT_SM, x, y + 8, width, body, 8);
}
