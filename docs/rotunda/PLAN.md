# Rotunda build plan

Build plan for the Rotunda design (see `DESIGN.md`, `DATA.md`, `screens/`).
Kept current as steps land. Target: X3 only, `pio run -e default`, portrait.

Baseline (untouched `develop` at 148e6e2b): `firmware.bin` 5,516,957 B of the
6,553,600 B `app0` partition (84.2 %); static RAM 69,240 B.

## How Rotunda fits the firmware

| Need | Existing mechanism reused |
| --- | --- |
| Theme selection | `CrossPointSettings::UI_THEME` + `UITheme::setTheme()` (`src/components/UITheme.cpp`). Rotunda adds `ROTUNDA = 5`. |
| Shared chrome | `BaseTheme` virtuals (`drawHeader`, `drawButtonMenu`, `drawSubHeader`, `fillBatteryIcon`) and `ThemeMetrics` → `uiThemeTokens()` for FreeInkUI lists. |
| Home | `ActivityManager::goHome()` (`src/activities/ActivityManager.cpp`) builds `RotundaHomeActivity` when the Rotunda theme is on. |
| Open an office | The reader's TOC jump (`currentSpineIndex` + `pendingAnchor`, `EpubReaderActivity.cpp` chapter-selection handler). A one-shot "open at TOC title" request is consumed in `loadBook()`. |
| Office kept | The reader's per-page progress save (`EpubReaderActivity::render`, after `renderContents`). One check per page change: last page of the office's TOC range → append to `said.tsv`. |
| RTC | `halClock.isAvailable()` / `halClock.localTime()` (`lib/hal/HalClock.h`). |
| SD files | `HalFile` via `Storage`; appends use `Storage.open(path, O_WRONLY \| O_CREAT \| O_APPEND)` as `PluginEvents.cpp` does. |
| Covers | `Epub::generateThumbBmpFromSource(h)` 1-bit dithered thumbs cached as `thumb_<h>.bmp` (`lib/Epub/Epub.cpp`), drawn with `BaseTheme::drawCoverThumbFill`. |
| Sleep | `SleepActivity` grayscale path (`renderBitmapSleepScreen`), new `ROTUNDA` sleep mode. |
| Fonts | `fontconvert.py` 1-bit headers, `build-font-ids.sh`, registration in `src/main.cpp`. |
| Tests | Host gtest suites under `test/` (CMake/CTest). |

## Steps

| # | Step | Files | RAM / flash | Verify on the X3 |
| --- | --- | --- | --- | --- |
| 1 | Baseline build | none | — | Flash, confirm stock firmware boots. |
| 2 | Office data module | `lib/Rotunda/*` (pure C++), `src/rotunda/RotundaStore.*` (SD glue), `test/rotunda_office/*` | No heap at rest. Per call: one 512 B line buffer on the heap, freed on return; `DayEntry` ~700 B owned by the caller. | Host tests against `sd-sample/`. |
| 3 | Theme chrome + fonts | `RotundaTheme.*`, `UITheme.cpp`, `CrossPointSettings.h`, `SettingsList.h`, `main.cpp`, `fontIds.h`, Inter headers | Flash: Inter at 4 sizes (measured below). RAM: none at rest. | Settings → Display → Theme → Rotunda; headers show dithered bar, lists use 56 px black rows. |
| 4 | Home | `RotundaHomeActivity.*`, `ActivityManager.cpp` | Activity ~1.5 KB (today's `DayEntry`, kept mask, continue-reading strings). | Home shows the menu + art pane; moving selection is a fast 1-bit refresh. |
| 5 | Today's Office | `TodayOfficeActivity.*`, `OfficeLauncher.*` | ~1 KB | Clock-selected office is focused; OK opens that office's chapter. Missing EPUB → message. |
| 6 | Mark kept | `OfficeReaderHook.*`, `EpubReaderActivity.*` | 0 B at rest; one 512 B buffer for the duplicate scan on the last page of an office only. | Read to the last page of Morning Prayer → a line appears in `/office/said.tsv`. Hold OK on a Friday → `friday devotion` line. |
| 7 | Calendar | `CalendarActivity.*` | 31 × 2 B month marks + 46 B streak bitmap | Pips/marks match `screens/calendar.png` with the sample data. |
| 8 | Lectionary | `LectionaryActivity.*` | One `DayEntry` + 6 strip dates | ◀ walks back across a month boundary; rows open the EPUB if present. |
| 9 | Book Clubs | `BookClubsActivity.*` | 4 × ~400 B club rows | Rows show covers, assignment, pages left; OK opens the book; hold OK opens notes. |
| 10 | Sleep screen | `SleepActivity.*`, settings | Cover BMP streamed from SD (row buffer only) | Settings → Sleep Screen → Now Praying; sleep shows cover, office, scrubber. |
| 11 | Reader chrome | `EpubReaderActivity.*`, `OfficeReaderHook.*` | 0 B at rest | In an office EPUB with Rotunda on: title bar shows passage + "n of m", foot shows minutes left. Other books unchanged. |
| 12 | Library flow | `LibraryFlowActivity.*`, `CoverFlowCache.*` | One source thumb (≤ 11 KB, 1-bit 236 × 354) while a side cover is skewed, freed right after; nothing at rest beyond the visible book list. | Library shows 7 covers; first visit generates `flow_l/r.bmp`; later visits draw from cache. |
| 13 | Release checks | — | — | Device checklist (see the step-13 report). |

## Where the design cannot be drawn exactly, and what Rotunda does instead

1. **Grey covers on in-app screens.** Home, Today, Clubs and Library covers use
   the existing 1-bit dithered thumbnails so every screen stays on the fast
   1-bit refresh (DESIGN.md: everything that moves is 1-bit). True 4-grey is
   used only on the sleep screen, rendered once. The dither looks grey at
   arm's length; it is not the 4-ink render in `screens/`.
2. **"Partial refresh of the two affected regions".** The display HAL's
   `FAST_REFRESH` is a differential 1-bit update of the whole frame; only
   changed pixels move. There is no windowed refresh API in `HalDisplay`, so
   Rotunda uses `FAST_REFRESH` for selection moves.
3. **Existing screens keep their UI font.** Settings, file browser, etc. get
   Rotunda geometry (title bar, 56 px rows, black selection) but keep the
   Ubuntu UI font, because the built-in Ubuntu fonts carry the Hebrew/Arabic
   coverage those screens rely on. Rotunda's own screens use Inter. FreeInkUI
   screens that draw their own header (Settings) get the 46 px band and 2 px
   rule but not the dither.
4. **Data file names.** The PDF says `.csv`; `DATA.md` (the contract) says
   tab-separated `.tsv`. Rotunda follows `DATA.md`.
5. **Noonday and Compline readings.** The index has only MP/EP columns.
   Today's Office shows the fixed BCP 1979 Noonday and Compline psalms from the
   render. Office durations (20/10/18/10 min) are fixed estimates.
6. **Shuffle Psalms.** Not in `DATA.md`. Rotunda opens a random TOC entry of
   `/office/psalter.epub` if present, otherwise shows a message. Added to `DATA.md`.
7. **Reader minutes left.** Estimated from the remaining pages of the office ×
   words on the current page ÷ 200 wpm; it is an estimate, not a word count.
8. **Pages left (Book Clubs).** Exact only for chapters whose layout cache
   exists; otherwise estimated from EPUB item sizes and the current chapter's
   bytes per page.
9. **Calendar rows.** Months that need six weeks get a sixth row; totals move down.

10. **Wake text.** The X3 wakes from deep sleep only on the power button
    (`gpio.verifyPowerButtonWakeup()` in `src/main.cpp`), so the sleep screen says
    "Press power to wake" instead of the render's "Hold OK to wake".
11. **Pips.** As drawn in `screens/calendar.png`, pips fill left to right by
    count (2 of 4 kept = two filled), not by which offices were kept.
12. **A–Z strip.** The library index sorts on the full title, so "The Cost of
    Discipleship" files under T (the render shows C). Jumps follow the real sort.
13. **Keys.** The X3 has two key pairs. Where a screen needs both "walk" and
    "focus", the front ◀/▶ walk (days in Calendar/Lectionary, books in Library)
    and the side keys move focus or jump (weeks in Calendar, reading rows in
    Lectionary, letters in Library). Elsewhere both pairs scroll.

## Status

All 13 steps are implemented on the `rotunda` branch. Nothing has been flashed
or checked on an X3 yet; every step still needs its device check.

## Measured costs

| Item | Cost |
| --- | --- |
| `firmware.bin` (`default`) | 5,655,349 B of 6,553,600 B (86.3 %), +138,392 B over baseline |
| Inter chrome fonts (6 faces, Latin-1 subset, kerning) | ~65 KB flash |
| Static RAM | 69,552 B, +312 B over baseline |
| Heap at rest | 0 B outside an open Rotunda screen |
| Per screen (heap, freed on exit) | Home ~1.2 KB · Today ~1 KB · Calendar ~0.2 KB · Lectionary ~1.7 KB · Clubs ~1.8 KB · Library: index reader + the visible book's strings |
| Per data call | one 640 B scratch block (file read-ahead + line buffer), freed before return; Home and Library also borrow a 1.46 KB club list while they open |
| Office reader hook | ~760 B, only while a Daily Office EPUB is open |
| Cover-flow generation | one 1-bit thumb (≤ 11 KB) while a strip is written, then freed |
