#include "RotundaLibraryActivity.h"

#include <Bitmap.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <LibraryBuilder.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>
#include <cstring>

#include "CrossPointSettings.h"
#include "activities/util/DetailsActivity.h"
#include "components/UITheme.h"
#include "components/themes/rotunda/RotundaChrome.h"
#include "rotunda/CoverFlowCache.h"
#include "rotunda/RotundaBooks.h"
#include "rotunda/RotundaStore.h"

namespace RC = RotundaChrome;
namespace CF = CoverFlowCache;

namespace {
constexpr int COVER_TOP = 96;
constexpr int CENTER_GAP = 18;
constexpr int SIDE_STEP = 22;
constexpr int TITLE_TOP = COVER_TOP + CF::CENTER_H + 44;
constexpr int STRIP_TOP = 650;
constexpr int HINT_TOP = 732;

// Side strip for a book without a usable cover: the strip's trapezoid in outline.
void drawSidePlaceholder(const GfxRenderer& renderer, const int x, const int y, const bool leftSide) {
  const int inset = (CF::CENTER_H - CF::SIDE_OUTER_H) / 2;
  const int outer = leftSide ? x : x + CF::SIDE_W - 1;
  const int inner = leftSide ? x + CF::SIDE_W - 1 : x;
  const int xs[4] = {outer, inner, inner, outer};
  const int ys[4] = {y + inset, y, y + CF::CENTER_H - 1, y + CF::CENTER_H - 1 - inset};
  renderer.fillPolygon(xs, ys, 4, false);
  for (int i = 0; i < 4; ++i) {
    const int j = (i + 1) % 4;
    renderer.drawLine(xs[i], ys[i], xs[j], ys[j], true);
  }
}

bool drawBmp(const GfxRenderer& renderer, const std::string& path, const int x, const int y) {
  if (path.empty()) return false;
  HalFile file;
  if (!Storage.openFileForRead("ROT", path, file)) return false;
  Bitmap bitmap(file);
  if (bitmap.parseHeaders() != BmpReaderError::Ok) return false;
  return renderer.drawBitmap(bitmap, x, y, bitmap.getWidth(), bitmap.getHeight());
}
}  // namespace

bool RotundaLibraryActivity::openIndex() {
  reader = makeUniqueNoThrow<IndexReader>();
  if (!reader) {
    LOG_ERR("ROT", "OOM: library index (%u bytes)", static_cast<unsigned>(sizeof(IndexReader)));
    return false;
  }
  if (reader->index.open(library::libraryIndexPath())) return true;
  reader->index.close();
  GUI.drawPopup(renderer, tr(STR_LIBRARY_REBUILDING));
  library::BuildStats stats;
  if (!library::buildLibraryIndex("/", stats, SETTINGS.libraryUseMetadata != 0) ||
      !reader->index.open(library::libraryIndexPath())) {
    LOG_ERR("ROT", "Cannot open the library index");
    return false;
  }
  return true;
}

void RotundaLibraryActivity::onEnter() {
  RotundaScreen::onEnter();
  if (openIndex()) count = reader->index.bookCount();

  // Letter table: one pass over the title-sorted rows.
  std::string title;
  for (int row = 0; row < count; ++row) {
    if (!reader->index.readRecord(reader->index.ordinalForRow(library::SortOrder::TitleAsc, row), reader->record))
      continue;
    if (!reader->index.readTitle(reader->record, title) && !reader->index.readName(reader->record, title)) continue;
    const int slot = letterSlot(title);
    if (!(letterMask & (1u << slot))) {
      letterMask |= 1u << slot;
      letterFirstRow[slot] = static_cast<uint16_t>(row);
    }
    if (reader->index.ioFailed()) break;
  }

  // ClubList (~1.7 KB) is over the stack budget; only the book paths are kept.
  auto clubs = makeUniqueNoThrow<rotunda::ClubList>();
  if (!clubs) {
    LOG_ERR("ROT", "OOM: club list (%u bytes)", static_cast<unsigned>(sizeof(rotunda::ClubList)));
  } else if (RotundaStore::readClubs(*clubs)) {
    for (int i = 0; i < clubs->count && i < 4; ++i) clubBooks[i] = clubs->clubs[i].book;
  }
  requestUpdate();
}

void RotundaLibraryActivity::onExit() {
  RotundaScreen::onExit();
  reader.reset();
}

int RotundaLibraryActivity::letterSlot(const std::string& title) {
  const char c = title.empty() ? '#' : title[0];
  if (c >= 'a' && c <= 'z') return c - 'a' + 1;
  if (c >= 'A' && c <= 'Z') return c - 'A' + 1;
  return 0;
}

bool RotundaLibraryActivity::bookAt(const int row, std::string& path, std::string* title, std::string* author) {
  path.clear();
  if (!reader || row < 0 || row >= count) return false;
  auto& idx = reader->index;
  if (!idx.readRecord(idx.ordinalForRow(library::SortOrder::TitleAsc, static_cast<uint16_t>(row)), reader->record) ||
      !idx.readPath(reader->record, path)) {
    return false;
  }
  if (title && !idx.readTitle(reader->record, *title)) idx.readName(reader->record, *title);
  if (author) idx.readAuthor(reader->record, *author);
  return true;
}

void RotundaLibraryActivity::jumpLetter(const int direction) {
  if (count == 0 || letterMask == 0) return;
  std::string title;
  std::string path;
  bookAt(selected, path, &title);
  int slot = letterSlot(title);
  for (int i = 0; i < LETTERS; ++i) {
    slot = (slot + direction + LETTERS) % LETTERS;
    if (letterMask & (1u << slot)) {
      selected = letterFirstRow[slot];
      requestUpdate();
      return;
    }
  }
}

void RotundaLibraryActivity::loop() {
  if (count > 0) {
    buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Right}, [this] {
      selected = ButtonNavigator::nextIndex(selected, count);
      requestUpdate();
    });
    buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Left}, [this] {
      selected = ButtonNavigator::previousIndex(selected, count);
      requestUpdate();
    });
    buttonNavigator.onPress({MappedInputManager::Button::Down}, [this] { jumpLetter(1); });
    buttonNavigator.onPress({MappedInputManager::Button::Up}, [this] { jumpLetter(-1); });
    if (confirmLongPressed()) {
      showDetails();
      return;
    }
    if (confirmReleased()) {
      std::string path;
      if (bookAt(selected, path)) RotundaBooks::open(path.c_str());
      return;
    }
  }
  if (backReleased()) activityManager.goHome();
}

void RotundaLibraryActivity::shelfLine(const std::string& path, char* out, const size_t cap) const {
  const char* shelf = nullptr;
  std::string folder;
  for (const auto& club : clubBooks) {
    if (!club.empty() && club == path) shelf = tr(STR_ROT_BOOK_CLUBS);
  }
  if (!shelf && RotundaBooks::isDailyOfficeEpub(path)) shelf = tr(STR_ROT_THE_DAILY_OFFICE);
  if (!shelf) {
    const size_t slash = path.find_last_of('/');
    const size_t prev =
        slash == std::string::npos || slash == 0 ? std::string::npos : path.find_last_of('/', slash - 1);
    folder = slash == std::string::npos || slash == 0 ? "/" : path.substr(prev + 1, slash - prev - 1);
    shelf = folder.c_str();
  }
  RotundaBooks::Position pos;
  if (RotundaBooks::readPosition(path.c_str(), pos) && pos.chapter > 0) {
    char progress[32];
    snprintf(progress, sizeof(progress), tr(STR_ROT_CHAPTER_PERCENT), pos.chapter, pos.percent);
    snprintf(out, cap, "%s \xC2\xB7 %s", shelf, progress);
  } else {
    snprintf(out, cap, "%s", shelf);
  }
}

void RotundaLibraryActivity::showDetails() {
  std::string path;
  DetailsSpec spec;
  if (!bookAt(selected, path, &spec.title, &spec.author)) return;
  char shelf[96];
  shelfLine(path, shelf, sizeof(shelf));
  spec.header = tr(STR_LIBRARY);
  spec.source = path;
  spec.metadata = shelf;
  spec.actionLabel = tr(STR_OPEN);
  auto details = makeUniqueNoThrow<DetailsActivity>(renderer, mappedInput, std::move(spec));
  if (!details) {
    LOG_ERR("ROT", "OOM: details");
    return;
  }
  startActivityForResult(std::move(details), [this, path](const ActivityResult& result) {
    if (!result.isCancelled) {
      RotundaBooks::open(path.c_str());
      return;
    }
    markFullRefresh();
    requestUpdate();
  });
}

void RotundaLibraryActivity::render(RenderLock&&) {
  const int w = renderer.getScreenWidth();
  const int h = renderer.getScreenHeight();
  // The first frame may generate thumbs and strips for seven books; the popup
  // goes up over the previous screen (drawPopup pushes it to the panel) before
  // the framebuffer is cleared for this frame.
  if (firstFrame && count > 0) {
    firstFrame = false;
    GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
  }
  renderer.clearScreen();
  char position[16] = "";
  if (count > 0) snprintf(position, sizeof(position), tr(STR_ROT_N_OF_M), selected + 1, count);
  RC::TitleBar bar;
  bar.title = tr(STR_LIBRARY);
  bar.left = RC::Glyph::None;
  bar.rightLabel = position;
  RC::drawTitleBar(renderer, bar);
  renderer.fillRect(0, RC::TITLE_BAR_HEIGHT, w, h - RC::TITLE_BAR_HEIGHT, true);

  if (count == 0) {
    const char* empty = tr(STR_ROT_NO_BOOKS);
    renderer.drawText(RC::FONT_ROW, (w - renderer.getTextWidth(RC::FONT_ROW, empty)) / 2, h / 2, empty, false);
    present();
    return;
  }

  const int centerX = (w - CF::CENTER_W) / 2;
  std::string path;
  // Side covers, farthest first so nearer ones overlap them.
  for (int d = SIDE_COUNT; d >= 1; --d) {
    if (selected - d >= 0 && bookAt(selected - d, path)) {
      const int x = centerX - CENTER_GAP - CF::SIDE_W - (d - 1) * SIDE_STEP;
      if (!drawBmp(renderer, CF::sidePath(path, true), x, COVER_TOP)) drawSidePlaceholder(renderer, x, COVER_TOP, true);
    }
    if (selected + d < count && bookAt(selected + d, path)) {
      const int x = centerX + CF::CENTER_W + CENTER_GAP + (d - 1) * SIDE_STEP;
      if (!drawBmp(renderer, CF::sidePath(path, false), x, COVER_TOP))
        drawSidePlaceholder(renderer, x, COVER_TOP, false);
    }
  }

  std::string title;
  std::string author;
  bookAt(selected, path, &title, &author);
  const Rect slot{centerX, COVER_TOP, CF::CENTER_W, CF::CENTER_H};
  renderer.fillRect(slot.x - 3, slot.y - 3, slot.width + 6, slot.height + 6, false);
  bool drawn = false;
  {
    HalFile file;
    const std::string thumb = CF::centerPath(path);
    if (!thumb.empty() && Storage.openFileForRead("ROT", thumb, file)) {
      Bitmap bitmap(file);
      if (bitmap.parseHeaders() == BmpReaderError::Ok) drawn = BaseTheme::drawCoverThumbFill(renderer, bitmap, slot);
    }
  }
  if (!drawn) RC::drawCover(renderer, nullptr, slot, false, title.c_str());
  renderer.drawRect(slot.x - 1, slot.y - 1, slot.width + 2, slot.height + 2, true);

  // Title, author, shelf and progress in white on black.
  const int textW = w - 32;
  const std::string fitted = renderer.truncatedText(RC::FONT_ROW, title.c_str(), textW);
  renderer.drawText(RC::FONT_ROW, (w - renderer.getTextWidth(RC::FONT_ROW, fitted.c_str())) / 2, TITLE_TOP,
                    fitted.c_str(), false);
  int y = TITLE_TOP + renderer.getLineHeight(RC::FONT_ROW) + 4;
  if (!author.empty()) {
    const std::string a = renderer.truncatedText(RC::FONT_SM, author.c_str(), textW);
    renderer.drawText(RC::FONT_SM, (w - renderer.getTextWidth(RC::FONT_SM, a.c_str())) / 2, y, a.c_str(), false);
  }
  y += renderer.getLineHeight(RC::FONT_SM) + 2;
  char shelf[96];
  shelfLine(path, shelf, sizeof(shelf));
  renderer.drawText(RC::FONT_SM, (w - renderer.getTextWidth(RC::FONT_SM, shelf)) / 2, y, shelf, false);
  y += renderer.getLineHeight(RC::FONT_SM) + 14;
  RotundaBooks::Position pos;
  const int permille = RotundaBooks::readPosition(path.c_str(), pos) ? pos.percent * 10 : 0;
  RC::drawBar(renderer, 90, y, w - 180, 8, permille, false);

  // A-Z jump strip.
  constexpr int stripW = 480;
  const int step = stripW / LETTERS;
  const int stripX = (w - stripW) / 2;
  const int current = letterSlot(title);
  for (int i = 0; i < LETTERS; ++i) {
    char letter[2] = {i == 0 ? '#' : static_cast<char>('A' + i - 1), '\0'};
    const bool has = letterMask & (1u << i);
    const auto style = has ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
    const int lw = renderer.getTextWidth(RC::FONT_XS, letter, style);
    const int cx = stripX + i * step + step / 2;
    renderer.drawText(RC::FONT_XS, cx - lw / 2, STRIP_TOP, letter, false, style);
    // Letters with no books are dimmed by dithering black back over them.
    if (!has) RC::fillDither(renderer, cx - lw / 2, STRIP_TOP, lw + 1, renderer.getLineHeight(RC::FONT_XS), 10);
    if (i == current) {
      renderer.drawRect(cx - step / 2, STRIP_TOP - 3, step, renderer.getLineHeight(RC::FONT_XS) + 4, 2, false);
    }
  }

  // Hints: "◀▶ flip  OK open  long-OK details".
  const char* flip = tr(STR_ROT_FLIP);
  const char* open = tr(STR_ROT_OK_OPEN);
  const char* details = tr(STR_ROT_LONG_OK_DETAILS);
  const int gap = 14;
  const int glyphs = 22;
  const int total = glyphs + renderer.getTextWidth(RC::FONT_XS, flip) + gap + renderer.getTextWidth(RC::FONT_XS, open) +
                    gap + renderer.getTextWidth(RC::FONT_XS, details);
  int x = (w - total) / 2;
  RC::drawTriangle(renderer, x, HINT_TOP + 4, 8, 10, true, false);
  RC::drawTriangle(renderer, x + 11, HINT_TOP + 4, 8, 10, false, false);
  x += glyphs;
  renderer.drawText(RC::FONT_XS, x, HINT_TOP, flip, false);
  x += renderer.getTextWidth(RC::FONT_XS, flip) + gap;
  renderer.drawText(RC::FONT_XS, x, HINT_TOP, open, false);
  x += renderer.getTextWidth(RC::FONT_XS, open) + gap;
  renderer.drawText(RC::FONT_XS, x, HINT_TOP, details, false);
  present();
}
