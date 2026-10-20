#include "RotundaSleep.h"

#include <Epub.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <JpegToBmpConverter.h>
#include <Logging.h>
#include <Memory.h>
#include <OfficeHours.h>
#include <PngToBmpConverter.h>

#include <algorithm>
#include <cstdio>

#include "RotundaStore.h"
#include "RotundaText.h"
#include "components/themes/rotunda/RotundaChrome.h"

namespace RC = RotundaChrome;

namespace RotundaSleep {

namespace {
constexpr int COVER_W = 300;
constexpr int COVER_H = 450;
constexpr int COVER_TOP = 72;
constexpr int SCRUBBER_X = 40;
constexpr int SCRUBBER_H = 12;
// Dither density for the elapsed part of the current office (dark grey look).
constexpr int ELAPSED_DENSITY = 10;
}  // namespace

std::string lockCoverPath(const std::string& epubPath) {
  if (epubPath.empty() || !Storage.exists(epubPath.c_str())) return {};
  auto epub = makeUniqueNoThrow<Epub>(epubPath, "/.crosspoint");
  if (!epub) {
    LOG_ERR("ROT", "OOM: lock cover EPUB");
    return {};
  }
  const std::string path = epub->getCachePath() + "/rotunda_lock.bmp";
  if (Storage.exists(path.c_str())) return path;
  if (!epub->load(true, true)) return {};
  const std::string href = epub->getCoverItemHref();
  const bool jpg = FsHelpers::hasJpgExtension(href);
  if (href.empty() || (!jpg && !FsHelpers::hasPngExtension(href))) return {};

  const std::string tmp = epub->getCachePath() + (jpg ? "/.lock_cover.jpg" : "/.lock_cover.png");
  if (!epub->extractItemToFile(href, tmp)) return {};
  bool ok = false;
  {
    HalFile in;
    HalFile out;
    if (Storage.openFileForRead("ROT", tmp, in) && Storage.openFileForWrite("ROT", path, out)) {
      ok = jpg ? JpegToBmpConverter::jpegFileToBmpStreamWithSize(in, out, COVER_W, COVER_H)
               : PngToBmpConverter::pngFileToBmpStreamWithSize(in, out, COVER_W, COVER_H);
    }
  }
  Storage.remove(tmp.c_str());
  if (!ok) {
    LOG_ERR("ROT", "Lock cover conversion failed for %s", epubPath.c_str());
    Storage.remove(path.c_str());
    return {};
  }
  return path;
}

Layout drawFrame(GfxRenderer& renderer) {
  RotundaStore::reloadHours();
  const auto now = RotundaStore::now();
  // The day row (~770 B) and said.tsv scan are over the stack budget.
  struct SleepData {
    rotunda::DayEntry day;
    rotunda::KeptScan scan;
    char epubPath[112];
  };
  auto data = makeUniqueNoThrow<SleepData>();
  if (!data) {
    LOG_ERR("ROT", "OOM: sleep screen data (%u bytes)", static_cast<unsigned>(sizeof(SleepData)));
    renderer.clearScreen();
    return Layout();
  }
  rotunda::DayEntry& day = data->day;
  rotunda::KeptScan& scan = data->scan;
  char* epubPath = data->epubPath;
  const bool hasDay = now.officeDay.valid() && RotundaStore::findDay(now.officeDay, day);
  const bool hasEpub =
      RotundaStore::resolveDayEpub(hasDay ? &day : nullptr, now.officeDay, epubPath, sizeof(data->epubPath));

  scan.today = now.officeDay;
  scan.year = now.officeDay.year;
  scan.month = now.officeDay.month;
  if (now.officeDay.valid()) RotundaStore::scanSaid(scan);

  // The office due: the clock's office, or the next one once it is kept.
  int office = static_cast<int>(now.current.office);
  while (office < rotunda::OFFICE_COUNT - 1 && (scan.todayMask & (1u << office))) ++office;

  const int w = renderer.getScreenWidth();
  renderer.clearScreen();
  char position[12];
  snprintf(position, sizeof(position), tr(STR_ROT_N_OF_M), office + 1, rotunda::OFFICE_COUNT);
  RC::TitleBar bar;
  bar.title = tr(STR_ROT_NOW_PRAYING);
  bar.leftSecond = RC::Glyph::Lock;
  bar.rightLabel = position;
  RC::drawTitleBar(renderer, bar);

  Layout layout;
  layout.coverSlot = Rect{(w - COVER_W) / 2, COVER_TOP, COVER_W, COVER_H};
  if (hasEpub) layout.coverPath = lockCoverPath(epubPath);
  const Rect& slot = layout.coverSlot;
  renderer.drawRect(slot.x - 6, slot.y - 6, slot.width + 12, slot.height + 12, 3, true);
  if (layout.coverPath.empty()) {
    // No cover: the frame holds the day in type instead.
    RC::drawCover(renderer, nullptr, slot, false, tr(STR_ROT_THE_DAILY_OFFICE));
  }

  int y = slot.y + slot.height + 20;
  const auto officeEnum = static_cast<rotunda::Office>(office);
  const char* name = RotundaText::officeName(officeEnum);
  renderer.drawText(RC::FONT_TITLE, (w - renderer.getTextWidth(RC::FONT_TITLE, name)) / 2, y, name);
  y += renderer.getLineHeight(RC::FONT_TITLE) + 2;

  const auto& hours = RotundaStore::hours();
  char hour[8];
  rotunda::formatHour12(hours.start[office], hour, sizeof(hour));
  char line[112];
  snprintf(line, sizeof(line), "%s \xC2\xB7 %s", hour, tr(STR_ROT_THE_DAILY_OFFICE));
  const auto centered = [&](const int font, const char* text) {
    const std::string fitted = renderer.truncatedText(font, text, w - 32);
    renderer.drawText(font, (w - renderer.getTextWidth(font, fitted.c_str())) / 2, y, fitted.c_str());
    y += renderer.getLineHeight(font) + 2;
  };
  centered(RC::FONT_SM, line);
  if (hasDay) {
    char week[48];
    snprintf(week, sizeof(week), tr(STR_ROT_WEEK_OF), day.proper);
    snprintf(line, sizeof(line), "%s \xC2\xB7 %s", RotundaText::weekdayName(rotunda::weekday(day.date)), week);
    centered(RC::FONT_SM, line);
    const char* mark = day.feast[0] ? day.feast : day.commemoration;
    if (*mark) centered(RC::FONT_XS, mark);
  }

  // Day scrubber: kept offices solid, the due office dithered up to the clock.
  const int scrubW = w - 2 * SCRUBBER_X;
  const int segW = scrubW / rotunda::OFFICE_COUNT;
  const int scrubY = std::max(y + 18, slot.y + slot.height + 150);
  renderer.drawRect(SCRUBBER_X, scrubY, scrubW, SCRUBBER_H, true);
  const int nowMinutes = now.hour * 60 + now.minute;
  for (int i = 0; i < rotunda::OFFICE_COUNT; ++i) {
    const int sx = SCRUBBER_X + i * segW;
    if (scan.todayMask & (1u << i)) {
      renderer.fillRect(sx, scrubY, segW, SCRUBBER_H, true);
    } else if (i == office && now.hasClock) {
      const int start = hours.start[i];
      const int end = i + 1 < rotunda::OFFICE_COUNT ? hours.start[i + 1] : 24 * 60;
      const int span = std::max(1, end - start);
      const int elapsed = std::clamp(nowMinutes - start, span / 10, span);
      RC::fillDither(renderer, sx + 1, scrubY + 1, (segW - 2) * elapsed / span, SCRUBBER_H - 2, ELAPSED_DENSITY);
    }
    if (i > 0) renderer.fillRect(sx - 1, scrubY - 4, 2, SCRUBBER_H + 8, true);
    rotunda::formatHour12(hours.start[i], hour, sizeof(hour));
    const int hw = renderer.getTextWidth(RC::FONT_XS, hour);
    renderer.drawText(RC::FONT_XS, sx + (segW - hw) / 2, scrubY + SCRUBBER_H + 10, hour);
  }

  const char* wake = tr(STR_ROT_PRESS_POWER_TO_WAKE);
  renderer.drawText(RC::FONT_XS, (w - renderer.getTextWidth(RC::FONT_XS, wake)) / 2, renderer.getScreenHeight() - 40,
                    wake);
  return layout;
}

}  // namespace RotundaSleep
