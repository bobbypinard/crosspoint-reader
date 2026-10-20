#include "RotundaClubsActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include <cstdio>
#include <cstring>

#include "components/themes/rotunda/RotundaChrome.h"
#include "rotunda/RotundaBooks.h"
#include "rotunda/RotundaStore.h"
#include "rotunda/RotundaText.h"

namespace RC = RotundaChrome;

namespace {
constexpr int LIST_TOP = RC::TITLE_BAR_HEIGHT + 2;
constexpr int CLUB_ROW_H = 150;
constexpr int COVER_W = 90;
constexpr int COVER_H = 135;
constexpr int TEXT_X = RC::ROW_TEXT_X + COVER_W + 18;

// First line of a plain-text notes file, used as its label.
void readFirstLine(const char* path, char* out, const size_t cap) {
  out[0] = '\0';
  if (!path || !*path) return;
  HalFile file;
  if (!Storage.openFileForRead("ROT", path, file)) return;
  size_t n = 0;
  while (n + 1 < cap) {
    const int c = file.read();
    if (c < 0 || c == '\n') break;
    if (c == '\r') continue;
    out[n++] = static_cast<char>(c);
  }
  out[n] = '\0';
  // Strip a UTF-8 byte-order mark.
  if (n >= 3 && static_cast<unsigned char>(out[0]) == 0xEF && static_cast<unsigned char>(out[1]) == 0xBB &&
      static_cast<unsigned char>(out[2]) == 0xBF) {
    memmove(out, out + 3, n - 2);
  }
}
}  // namespace

void RotundaClubsActivity::onEnter() {
  RotundaScreen::onEnter();
  RotundaStore::readClubs(clubs);
  for (int i = 0; i < clubs.count; ++i) {
    const auto& c = clubs.clubs[i];
    RotundaBooks::assignmentProgress(c.book, c.startToc, c.endToc, status[i].pagesLeft, status[i].permille);
    readFirstLine(c.notes, status[i].notesTitle, sizeof(status[i].notesTitle));
  }
  requestUpdate();
}

void RotundaClubsActivity::loop() {
  if (clubs.count > 0) {
    buttonNavigator.onNext([this] {
      selected = ButtonNavigator::nextIndex(selected, clubs.count);
      requestUpdate();
    });
    buttonNavigator.onPrevious([this] {
      selected = ButtonNavigator::previousIndex(selected, clubs.count);
      requestUpdate();
    });
    if (confirmLongPressed()) {
      const char* notes = clubs.clubs[selected].notes;
      if (notes[0] && Storage.exists(notes)) RotundaBooks::open(notes);
      return;
    }
    if (confirmReleased()) {
      const char* book = clubs.clubs[selected].book;
      if (book[0] && Storage.exists(book)) RotundaBooks::open(book);
      return;
    }
  }
  if (backReleased()) activityManager.goHome();
}

void RotundaClubsActivity::render(RenderLock&&) {
  renderer.clearScreen();
  char count[12];
  const int empty = rotunda::ClubList::MAX_CLUBS - clubs.count;
  if (empty > 0) {
    snprintf(count, sizeof(count), "%d + %d", clubs.count, empty);
  } else {
    snprintf(count, sizeof(count), "%d", clubs.count);
  }
  RC::TitleBar bar;
  bar.title = tr(STR_ROT_BOOK_CLUBS);
  bar.left = RC::Glyph::None;
  bar.rightLabel = count;
  RC::drawTitleBar(renderer, bar);

  for (int i = 0; i < clubs.count; ++i) renderClub(i, LIST_TOP + i * CLUB_ROW_H);
  if (clubs.count < rotunda::ClubList::MAX_CLUBS) renderEmptySlot(LIST_TOP + clubs.count * CLUB_ROW_H);
  present();
}

void RotundaClubsActivity::renderClub(const int index, const int y) {
  const int w = renderer.getScreenWidth();
  const auto& club = clubs.clubs[index];
  const auto& st = status[index];
  const bool focused = index == selected;
  const bool ink = !focused;
  renderer.fillRect(0, y, w, CLUB_ROW_H - 1, focused);
  renderer.drawLine(0, y + CLUB_ROW_H - 1, w, y + CLUB_ROW_H - 1, true);

  RC::drawCover(renderer, club.book, Rect{RC::ROW_TEXT_X, y + 8, COVER_W, COVER_H}, true, club.name);

  const int textW = w - TEXT_X - 40;
  RC::drawTextFit(renderer, RC::FONT_ROW, TEXT_X, y + 10, textW, club.name, ink);
  RC::drawTextFit(renderer, RC::FONT_SM, TEXT_X, y + 42, textW, club.assignment, ink);

  char meets[24] = "";
  if (club.meets.valid()) RotundaText::formatMeetingDay(club.meets, meets, sizeof(meets));
  char line[64];
  const bool caughtUp = club.endToc <= 0 || st.pagesLeft == 0;
  if (caughtUp) {
    snprintf(line, sizeof(line), tr(STR_ROT_CAUGHT_UP_MEETS), meets);
  } else if (st.pagesLeft > 0) {
    snprintf(line, sizeof(line), tr(STR_ROT_PAGES_MEETS), st.pagesLeft, meets);
  } else {
    snprintf(line, sizeof(line), tr(STR_ROT_MEETS), meets);
  }
  if (meets[0] || !caughtUp) RC::drawTextFit(renderer, RC::FONT_SM, TEXT_X, y + 66, textW, line, ink);

  RC::drawBar(renderer, TEXT_X, y + 96, w - TEXT_X - 48, 10, caughtUp ? 1000 : st.permille, ink);
  RC::drawTextFit(renderer, RC::FONT_XS, TEXT_X, y + 114, textW, st.notesTitle, ink);
  RC::drawChevron(renderer, w - 22, y + CLUB_ROW_H / 2, ink);
}

void RotundaClubsActivity::renderEmptySlot(const int y) {
  const int x = RC::ROW_TEXT_X;
  renderer.drawRect(x, y + 20, COVER_W, 110, true);
  renderer.fillRect(x + COVER_W / 2 - 1, y + 20 + 55 - 10, 2, 20, true);
  renderer.fillRect(x + COVER_W / 2 - 10, y + 20 + 55 - 1, 20, 2, true);
  renderer.drawText(RC::FONT_ROW, TEXT_X, y + 24, tr(STR_ROT_NEW_CLUB));
  RC::drawTextFit(renderer, RC::FONT_SM, TEXT_X, y + 56, renderer.getScreenWidth() - TEXT_X - 16,
                  tr(STR_ROT_NEW_CLUB_HINT));
}
