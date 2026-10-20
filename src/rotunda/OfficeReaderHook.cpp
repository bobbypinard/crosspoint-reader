#include "OfficeReaderHook.h"

#include <Epub.h>
#include <Epub/Section.h>
#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>
#include <OfficeIndex.h>

#include <algorithm>
#include <cstdio>

#include "CrossPointSettings.h"
#include "RotundaStore.h"
#include "components/themes/rotunda/RotundaChrome.h"

namespace RC = RotundaChrome;

namespace {
// Markup-inclusive EPUB bytes per spoken word, and DESIGN's reading pace.
constexpr int BYTES_PER_WORD = 7;
constexpr int WORDS_PER_MINUTE = 200;
}  // namespace

std::unique_ptr<OfficeReaderHook> OfficeReaderHook::create(const Epub& epub) {
  rotunda::Date date;
  if (!rotunda::parseOfficeEpubDate(epub.getPath().c_str(), date)) return nullptr;
  auto hook = makeUniqueNoThrow<OfficeReaderHook>();
  if (!hook) {
    LOG_ERR("ROT", "OOM: office reader hook (%u bytes)", static_cast<unsigned>(sizeof(OfficeReaderHook)));
    return nullptr;
  }
  hook->bookDate = date;
  const int total = std::min(epub.getTocItemsCount(), rotunda::OfficeToc::MAX_TOC);
  for (int i = 0; i < total; ++i) {
    const auto item = epub.getTocItem(i);
    hook->toc.add(item.title.c_str(), item.spineIndex, item.level, item.anchor.empty());
  }
  return hook;
}

bool OfficeReaderHook::chromeActive(const GfxRenderer& renderer) {
  return SETTINGS.uiTheme == CrossPointSettings::ROTUNDA && renderer.getOrientation() == GfxRenderer::Portrait;
}

void OfficeReaderHook::refreshBounds(const Epub& epub, const Section& section, const int spine) {
  if (toc.boundsSpine() == spine && boundsPages == section.pageCount) return;
  boundsPages = section.pageCount;
  toc.resetBounds(spine);
  for (int i = 0; i < toc.count(); ++i) {
    if (toc.spine(i) != spine || toc.atTop(i)) continue;
    // Anchors not laid out yet stay unknown until the build reaches them.
    const auto page = section.findAnchor(epub.getTocItem(i).anchor);
    if (page.has_value()) toc.addBound(i, *page);
  }
}

void OfficeReaderHook::onPageShown(const Epub& epub, const Section& section, const int spine) {
  refreshBounds(epub, section, spine);
  const int page = section.currentPage;
  const int office = toc.officeAtPage(spine, page);
  if (office < 0 || (loggedMask & (1u << office))) return;
  if (!toc.isOfficeEnd(spine, page, section.pageCount, section.isBuildComplete(), epub.getSpineItemsCount())) return;
  // The book's date is the office day: Compline read after midnight still belongs to it.
  if (RotundaStore::appendKept(bookDate, static_cast<rotunda::Office>(office), false)) {
    loggedMask |= static_cast<uint8_t>(1u << office);
  }
}

int OfficeReaderHook::minutesFor(const Epub& epub, const Section& section, const int spine, const int office,
                                 const bool before) const {
  const int pages = std::max<int>(1, section.estimatedTotalPages());
  const size_t spineStart = spine > 0 ? epub.getCumulativeSpineItemSize(spine - 1) : 0;
  const size_t spineBytes = epub.getCumulativeSpineItemSize(spine) - spineStart;
  const size_t bytesPerPage = spineBytes / static_cast<size_t>(pages);

  // Pages of this office inside the current spine item, before or after the current page.
  size_t bytes = 0;
  bool reachedEdge = true;
  for (int p = section.currentPage + (before ? -1 : 1); p >= 0 && p < pages; p += before ? -1 : 1) {
    if (toc.officeAtPage(spine, p) != office) {
      reachedEdge = false;
      break;
    }
    bytes += bytesPerPage;
  }
  // Whole neighbouring spine items that belong to the same office.
  for (int s = before ? spine - 1 : spine + 1; reachedEdge && s >= 0 && s < epub.getSpineItemsCount();
       s += before ? -1 : 1) {
    if (toc.office(toc.tocAtSpineStart(s)) != office) break;
    const size_t start = s > 0 ? epub.getCumulativeSpineItemSize(s - 1) : 0;
    bytes += epub.getCumulativeSpineItemSize(s) - start;
  }
  return static_cast<int>((bytes / BYTES_PER_WORD + WORDS_PER_MINUTE / 2) / WORDS_PER_MINUTE);
}

void OfficeReaderHook::drawChrome(const GfxRenderer& renderer, const Epub& epub, const Section& section,
                                  const int spine) {
  refreshBounds(epub, section, spine);
  const int entry = toc.tocAtPage(spine, section.currentPage);
  const int office = toc.office(entry);

  // Title and "n of m" among the entry's siblings; cached per TOC entry.
  if (entry != chromeToc) {
    chromeToc = entry;
    chromeTitle[0] = '\0';
    if (entry >= 0) rotunda::copyField(chromeTitle, sizeof(chromeTitle), epub.getTocItem(entry).title.c_str());
    toc.siblingPosition(entry, chromeIndex, chromeTotal);
  }

  char position[16] = "";
  if (chromeTotal > 1) snprintf(position, sizeof(position), tr(STR_ROT_N_OF_M), chromeIndex, chromeTotal);
  RC::TitleBar bar;
  bar.title = chromeTitle[0] ? chromeTitle : epub.getTitle().c_str();
  bar.rightLabel = position;
  bar.showClock = false;
  RC::drawTitleBar(renderer, bar);

  if (office < 0) return;
  const int minutesIn = minutesFor(epub, section, spine, office, true);
  const int minutesLeft = minutesFor(epub, section, spine, office, false);
  const int w = renderer.getScreenWidth();
  const int h = renderer.getScreenHeight();
  const int x = 24;
  const int barW = w - 2 * x;
  const int barY = h - BOTTOM_INSET + 6;
  renderer.fillRect(0, h - BOTTOM_INSET, w, BOTTOM_INSET, false);
  const int total = minutesIn + minutesLeft;
  RC::drawScrubber(renderer, x, barY, barW, 16, total > 0 ? minutesIn * 1000 / total : 0);
  char in[24];
  char left[24];
  snprintf(in, sizeof(in), tr(STR_ROT_MIN_IN), minutesIn);
  snprintf(left, sizeof(left), tr(STR_ROT_MIN_LEFT), minutesLeft);
  const int labelY = barY + 24;
  renderer.drawText(RC::FONT_SM, x, labelY, in);
  RC::drawTextRight(renderer, RC::FONT_SM, x + barW, labelY, left);
}
