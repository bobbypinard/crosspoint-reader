# Rotunda build prompts for Claude Code

Paste these one at a time, in order. Wait until each step builds, you have
flashed it, and it looks right on the device before you send the next one.
Steps marked **(plan mode)** go in after pressing Shift+Tab until the prompt
shows plan mode.

---

## 0 · Orientation and build plan (plan mode)

```
Read AGENTS.md, SCOPE.md and docs/rotunda/FORK.md, then everything in
docs/rotunda/ (DESIGN.md, DATA.md, all eight PNGs in screens/, and the spec PDF).

Then survey how the existing firmware does the things Rotunda needs, citing paths
and line numbers:
- the theme system (src/components/UITheme.*, src/components/themes/BaseTheme.*,
  one existing theme such as lyra/) and how a theme is selected in Settings
- the Home activity (src/activities/home/HomeActivity.*), UiListActivity and the
  activity manager (docs/activity-manager.md)
- the sleep screen (src/activities/boot_sleep/SleepActivity.*)
- the reader: how an EPUB is opened at a given TOC entry, and where the reader
  knows it has reached the end of a chapter
- the RTC (lib/hal/HalClock.*) and how the X3 reports it is available
- SD file access through the HAL, and how covers are cached (HomeCoverCache)
- how UI fonts are generated and registered (lib/EpdFont/scripts/convert-builtin-fonts.sh,
  lib/EpdFont/builtinFonts/, src/fontIds.h)
- host unit tests (test/, `pio run -t unit-tests`)

Write docs/rotunda/PLAN.md: an ordered list of small steps that delivers the
eight screens in DESIGN.md, each step naming the files it adds or changes, the
RAM and flash it costs, and how I verify it on the X3. Prefer extending the
existing theme and list components over new frameworks. Flag anything in the
design that the hardware or the code cannot do as drawn, and propose the
closest workable alternative. Do not change any firmware code in this step.
```

## 1 · Baseline build of the untouched fork

```
Check the toolchain the way docs/contributing/getting-started.md describes and
build the unchanged firmware with `pio run -e default`, using compact build
output. Tell me the firmware path, its size against the app partition in
partitions.csv, and the exact command to flash it to my X3 over the pogo-pin
USB cable on Linux. Do not flash it yourself.
```

## 2 · Office data module (no UI yet)

```
Implement step 2 of docs/rotunda/PLAN.md: a small Rotunda data module that reads
and writes the SD files in docs/rotunda/DATA.md:
- settings.tsv (office hours, with the defaults from DATA.md)
- index/YYYY-MM.tsv (look up one day; list a month's rank/feast marks)
- said.tsv (append a kept office; count kept offices per day for a month;
  streak; full days; Fridays kept)
- clubs.tsv (up to four clubs)
- currentOffice(time) using the windows in DESIGN.md, including Compline after
  midnight counting toward the previous day

Read files line by line; do not load a whole month into RAM as strings. Add host
unit tests that run against docs/rotunda/sd-sample/. Show me the test results
and the heap cost of each call.
```

## 3 · Rotunda theme chrome

```
Implement the Rotunda theme chrome from DESIGN.md as a theme selectable in
Settings: the dithered 46 px title bar, 56 px list rows with chevrons, black
selection rows (pure 1-bit), the split-screen pane layout, the scrubber, pips,
star and fish marks. Bake Inter at the four sizes in DESIGN.md through the
repo's font pipeline and tell me the flash cost. Existing screens should pick up
the new chrome without other changes. Compare against docs/rotunda/screens/ and
tell me where the result will differ.
```

## 4 · Home

```
Build the Rotunda Home screen to match docs/rotunda/screens/home.png: the
seven-row menu on the left and the art pane on the right that follows the
highlighted row. For the Today's Office row the pane shows today's EPUB cover,
date, Proper, the four office hours with ✓ kept and ◀ due, and the
commemoration from the index. Add the "Continue reading" strip for the last
non-office book. Rows that are not built yet can open a placeholder. Moving the
selection must use a partial 1-bit refresh.
```

## 5 · Today's Office

```
Build Today's Office to match docs/rotunda/screens/today.png. Find today's EPUB
from the index row (fall back to the filename pattern in DATA.md), show the
header block and the four office rows with psalms, lessons and status. On entry,
select the office returned by currentOffice(). OK opens the EPUB directly at
that office's TOC entry (titles in DATA.md). If today's EPUB is missing, show a
clear message instead of crashing.
```

## 6 · Marking offices as kept

```
When the reader reaches the last page of an office chapter in a Daily Office
EPUB, append that office to /office/said.tsv with the office day and local
time, once per office per day. On a Friday, holding OK on Today's Office logs
the Friday devotion. Keep the hook small and outside the hot page-render path.
Tell me how to test it quickly on the device.
```

## 7 · Calendar

```
Build the monthly Calendar to match docs/rotunda/screens/calendar.png: pips per
day from said.tsv, star and fish from the index rank, today inverted, and the
month totals (offices kept, full days, streak, Fridays kept). ◀/▶ walk days;
hold to change month. OK on a day opens that day in the Lectionary screen.
```

## 8 · Lectionary archive

```
Build the Lectionary archive to match docs/rotunda/screens/lectionary.png: a
strip of recent days that ◀ walks back through, and for the selected day its
Proper, year, commemoration, morning and evening psalms and lessons, and collect,
all from the index files. Each reading row opens the EPUB at the matching office
if the file is on the card; the black bar opens the whole day. Past days must
work even when their EPUB has been deleted.
```

## 9 · Book Clubs

```
Build Book Clubs to match docs/rotunda/screens/clubs.png from /clubs/clubs.tsv:
up to four rows with the book's cached cover, name, assignment, pages left to
the end of end_toc, meeting date, progress bar and notes file. OK opens the book
at the reader's saved position; holding OK opens the notes file. Show an empty
"New Club" slot when fewer than four are set.
```

## 10 · Sleep screen

```
Build the "Now Praying" sleep screen to match docs/rotunda/screens/lock.png:
today's cover, the office that will be due, the commemoration, and the
four-segment day scrubber. Render it once on the way into sleep with the grey
waveform. No reflections. Make it a sleep-screen option alongside the existing
ones.
```

## 11 · Reader chrome

```
In the reader, when the book is a Daily Office EPUB and the Rotunda theme is
on, show the Rotunda title bar (current psalm or lesson and "1 of 3") and the
minutes-left scrubber from docs/rotunda/screens/reader.png. Estimate minutes
from words left at about 200 words per minute. Other books keep the normal
reader chrome.
```

## 12 · Library (flat Cover Flow)

```
Build the Library to match docs/rotunda/screens/library.png: the centre cover
flat, three skewed covers each side, title, author, shelf, progress, and an A–Z
jump strip. Skew each cover once when it is first shown and cache the result on
the SD card next to the existing cover cache; never transform covers at page-turn
time. If this costs too much RAM or time on the C3, propose a simpler layout
before building it.
```

## 13 · Release build and checks

```
Build a release image for the X3, run the host tests, run ./bin/clang-format-fix -g
and pio check, and give me a device checklist covering every Rotunda screen,
the office windows around 12:00, 16:30, 20:00 and midnight, a missing EPUB, an
empty clubs file, and a full refresh after 20 page turns. Then list what is
left from PLAN.md.
```

---

## Handy follow-ups

**Something looks wrong on the device**
```
On the X3, <screen> shows <what you saw>; it should look like
docs/rotunda/screens/<file>.png. Here is a photo: <attach>. Find the cause in the
code before changing anything, and tell me the fix and how to check it.
```

**Pull in upstream CrossPoint changes**
```
Fetch upstream, merge upstream/develop into rotunda, resolve conflicts keeping
Rotunda behaviour, rebuild and run the host tests. Summarise what upstream
changed that affects Rotunda. Do not push.
```
