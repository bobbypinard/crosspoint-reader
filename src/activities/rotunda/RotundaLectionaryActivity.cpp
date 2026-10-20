#include "RotundaLectionaryActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <cstdio>

#include "components/themes/rotunda/RotundaChrome.h"
#include "rotunda/RotundaBooks.h"
#include "rotunda/RotundaText.h"

namespace RC = RotundaChrome;

namespace {
constexpr int STRIP_DAYS = 6;
constexpr int STRIP_TOP = RC::TITLE_BAR_HEIGHT + 10;
constexpr int BOX_W = 80;
constexpr int BOX_H = 62;
constexpr int BOX_STEP = 84;
constexpr int BOX_X = 16;
constexpr int READING_ROW_H = 38;
constexpr int BAR_H = 48;
constexpr int BAR_BOTTOM_MARGIN = 16;
}  // namespace

void RotundaLectionaryActivity::onEnter() {
  RotundaScreen::onEnter();
  const auto now = RotundaStore::now();
  today = now.officeDay.valid() ? now.officeDay : selected;
  if (!selected.valid()) selected = today;
  stripEnd = rotunda::dayNumber(selected) > rotunda::dayNumber(today) ? selected : today;
  if (rotunda::dayNumber(selected) < rotunda::dayNumber(stripEnd) - (STRIP_DAYS - 1)) {
    stripEnd = rotunda::addDays(selected, STRIP_DAYS - 1);
  }
  loadDay();
  requestUpdate();
}

void RotundaLectionaryActivity::loadDay() {
  hasDay = RotundaStore::findDay(selected, day);
  epubPresent = RotundaStore::resolveDayEpub(hasDay ? &day : nullptr, selected, epubPath, sizeof(epubPath));

  rowCount = 0;
  const auto add = [this](const Section section, const StrId label, const char* value) {
    if (rowCount >= MAX_ROWS || !value || !*value) return;
    ReadingRow& row = rows[rowCount++];
    row.section = section;
    row.label = label;
    rotunda::copyField(row.value, sizeof(row.value), value);
  };
  if (hasDay) {
    char text[72];
    rotunda::formatPsalms(day.mpPsalms, nullptr, text, sizeof(text));
    add(Section::Morning, StrId::STR_ROT_PSALMS, text);
    static constexpr StrId LESSON_LABELS[3] = {StrId::STR_ROT_FIRST_LESSON, StrId::STR_ROT_SECOND_LESSON,
                                               StrId::STR_ROT_LESSON};
    for (int i = 0; i < 3 && rotunda::lessonAt(day.mpLessons, i, text, sizeof(text)); ++i) {
      add(Section::Morning, LESSON_LABELS[i], text);
    }
    rotunda::formatPsalms(day.epPsalms, nullptr, text, sizeof(text));
    add(Section::Evening, StrId::STR_ROT_PSALMS, text);
    // One evening reading is the Gospel; feasts appoint two lessons.
    char second[72];
    const bool twoLessons = rotunda::lessonAt(day.epLessons, 1, second, sizeof(second));
    for (int i = 0; i < 3 && rotunda::lessonAt(day.epLessons, i, text, sizeof(text)); ++i) {
      add(Section::Evening, twoLessons ? LESSON_LABELS[i] : StrId::STR_ROT_THE_GOSPEL, text);
    }
    add(Section::Collect, StrId::STR_ROT_COLLECT_OF_DAY, day.collect);
    if (day.rank == rotunda::Rank::Fast) add(Section::Collect, StrId::STR_ROT_FRIDAY, tr(STR_ROT_SPECIAL_DEVOTION));
  }
  focus = rowCount;  // the open bar
}

void RotundaLectionaryActivity::stepDay(const int delta) {
  selected = rotunda::addDays(selected, delta);
  const int32_t s = rotunda::dayNumber(selected);
  const int32_t end = rotunda::dayNumber(stripEnd);
  if (s > end) stripEnd = selected;
  if (s < end - (STRIP_DAYS - 1)) stripEnd = rotunda::addDays(selected, STRIP_DAYS - 1);
  loadDay();
  requestUpdate();
}

void RotundaLectionaryActivity::loop() {
  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Left}, [this] { stepDay(-1); });
  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Right}, [this] { stepDay(1); });
  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Down}, [this] {
    focus = ButtonNavigator::nextIndex(focus, rowCount + 1);
    requestUpdate();
  });
  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Up}, [this] {
    focus = ButtonNavigator::previousIndex(focus, rowCount + 1);
    requestUpdate();
  });

  if (confirmReleased()) {
    openFocused();
    return;
  }
  if (backReleased()) {
    if (pushed) {
      finish();
    } else {
      activityManager.goHome();
    }
  }
}

void RotundaLectionaryActivity::openFocused() {
  if (!epubPresent) return;
  if (focus >= rowCount) {
    RotundaBooks::open(epubPath);
    return;
  }
  const Section section = rows[focus].section;
  RotundaBooks::openOffice(epubPath, section == Section::Evening ? rotunda::Office::Evening : rotunda::Office::Morning);
}

void RotundaLectionaryActivity::render(RenderLock&&) {
  renderer.clearScreen();
  RC::TitleBar bar;
  bar.title = tr(STR_ROT_LECTIONARY);
  bar.left = RC::Glyph::None;
  bar.showClock = false;
  RC::drawTitleBar(renderer, bar);
  renderStrip(STRIP_TOP);
  renderDetails(STRIP_TOP + BOX_H + 22);

  // Black action bar: open the whole day.
  const int w = renderer.getScreenWidth();
  const int barY = renderer.getScreenHeight() - BAR_BOTTOM_MARGIN - BAR_H;
  const bool barFocused = focus >= rowCount;
  char label[48];
  if (epubPresent) {
    char date[24];
    RotundaText::formatShortDate(selected, date, sizeof(date));
    snprintf(label, sizeof(label), tr(STR_ROT_OPEN_DAY_EPUB), date);
  } else {
    rotunda::copyField(label, sizeof(label), tr(STR_ROT_EPUB_NOT_ON_CARD));
  }
  // Focused: solid black. Unfocused: outlined, so the focused row reads as the only black band.
  if (barFocused) {
    renderer.fillRect(RC::ROW_TEXT_X, barY, w - 2 * RC::ROW_TEXT_X, BAR_H, true);
  } else {
    renderer.drawRect(RC::ROW_TEXT_X, barY, w - 2 * RC::ROW_TEXT_X, BAR_H, 2, true);
  }
  const int tw = renderer.getTextWidth(RC::FONT_SM, label, EpdFontFamily::BOLD);
  renderer.drawText(RC::FONT_SM, (w - tw) / 2, RC::centeredTextY(renderer, RC::FONT_SM, barY, BAR_H), label,
                    !barFocused, EpdFontFamily::BOLD);
  present();
}

void RotundaLectionaryActivity::renderStrip(const int top) {
  RC::drawChevron(renderer, 6, top + BOX_H / 2, true, true);
  const rotunda::Date first = rotunda::addDays(stripEnd, -(STRIP_DAYS - 1));
  for (int i = 0; i < STRIP_DAYS; ++i) {
    const rotunda::Date d = rotunda::addDays(first, i);
    const int x = BOX_X + i * BOX_STEP;
    const bool isSelected = d == selected;
    if (isSelected) {
      renderer.fillRect(x, top, BOX_W, BOX_H, true);
    } else {
      renderer.drawRect(x, top, BOX_W, BOX_H, 2, true);
    }
    const char* name = RotundaText::weekdayName(rotunda::weekday(d), true);
    const int nw = renderer.getTextWidth(RC::FONT_XS, name);
    renderer.drawText(RC::FONT_XS, x + (BOX_W - nw) / 2, top + 6, name, !isSelected);
    char number[4];
    snprintf(number, sizeof(number), "%d", d.day);
    const int dw = renderer.getTextWidth(RC::FONT_ROW, number);
    renderer.drawText(RC::FONT_ROW, x + (BOX_W - dw) / 2, top + 26, number, !isSelected);
  }
}

int RotundaLectionaryActivity::renderDetails(int top) {
  const int w = renderer.getScreenWidth();
  const int left = RC::ROW_TEXT_X;
  const int right = w - RC::ROW_TEXT_X;
  char line[112];
  RotundaText::formatLongDate(selected, line, sizeof(line));
  RC::drawTextFit(renderer, RC::FONT_TITLE, left, top, right - left, line);
  top += renderer.getLineHeight(RC::FONT_TITLE) + 2;
  if (!hasDay) {
    renderer.drawText(RC::FONT_SM, left, top, tr(STR_ROT_NO_INDEX));
    return top + renderer.getLineHeight(RC::FONT_SM);
  }
  char week[48];
  snprintf(week, sizeof(week), tr(STR_ROT_WEEK_OF), day.proper);
  snprintf(line, sizeof(line), "%s \xC2\xB7 %s", week, day.year);
  RC::drawTextFit(renderer, RC::FONT_SM, left, top, right - left, line);
  top += renderer.getLineHeight(RC::FONT_SM);
  const char* mark = day.feast[0] ? day.feast : day.commemoration;
  if (*mark) {
    RC::drawTextFit(renderer, RC::FONT_SM, left, top, right - left, mark);
    top += renderer.getLineHeight(RC::FONT_SM);
  }
  top += 12;

  Section current = Section::Collect;
  bool firstRow = true;
  for (int i = 0; i < rowCount; ++i) {
    const ReadingRow& row = rows[i];
    if (firstRow || row.section != current) {
      if (!firstRow) top += 10;
      current = row.section;
      firstRow = false;
      const StrId heading = current == Section::Morning   ? StrId::STR_ROT_SECTION_MORNING
                            : current == Section::Evening ? StrId::STR_ROT_SECTION_EVENING
                                                          : StrId::STR_ROT_SECTION_COLLECT;
      renderer.drawText(RC::FONT_XS, left, top + 4, I18N.get(heading), true, EpdFontFamily::BOLD);
      top += renderer.getLineHeight(RC::FONT_XS) + 4;
    }
    const bool focused = i == focus;
    const bool ink = !focused;
    if (focused) renderer.fillRect(0, top, w, READING_ROW_H, true);
    const int ty = RC::centeredTextY(renderer, RC::FONT_SM, top, READING_ROW_H);
    const char* label = I18N.get(row.label);
    renderer.drawText(RC::FONT_SM, left, ty, label, ink);
    const int labelW = renderer.getTextWidth(RC::FONT_SM, label) + 16;
    const int valueRight = right - 18;
    const int valueW = valueRight - left - labelW;
    if (renderer.getTextWidth(RC::FONT_SM, row.value, EpdFontFamily::BOLD) <= valueW) {
      RC::drawTextRight(renderer, RC::FONT_SM, valueRight, ty, row.value, ink, true);
    } else {
      RC::drawTextFit(renderer, RC::FONT_SM, left + labelW, ty, valueW, row.value, ink, true);
    }
    RC::drawChevron(renderer, right - 4, top + READING_ROW_H / 2, ink);
    top += READING_ROW_H;
    if (!focused) renderer.drawLine(left, top - 1, right, top - 1, true);
  }
  return top;
}
