# Rotunda Psalter — design spec for the firmware

Rotunda is a Daily Office front end for the Xteink X3, styled after the
click-wheel music player. This file is the build reference. The full sheet is
`rotunda-spec-revB.pdf`; native renders of every screen are in `screens/`
(528 × 792, four inks — open them and match them).

## Panel and inks

- X3 portrait, 528 × 792 px. Portrait only for Rotunda screens.
- Four inks: `#000000`, `#555555`, `#AAAAAA`, `#FFFFFF`.
- **Grey only appears in covers and the title-bar dither.** Everything that
  moves (selection, pips, progress) is pure black/white so it can use the fast
  1-bit partial refresh.
- No animation, no reflections. Anything that blinked in the original is static.

## Type

Inter (OFL) for chrome, baked through the repo's existing font pipeline.
Keep to four sizes to save flash:

| Token | Size / weight | Used for |
| --- | --- | --- |
| `xs` | 14 px, 500 | footnotes, counters, legends |
| `sm` | 17 px, 500 | secondary lines, psalms/lessons |
| `row` | 23 px, 600 | list rows, office names |
| `title` | 28 px, 700 | screen titles, lock-screen headline |

The reader body text is the reader's own font setting, not part of the theme.

## Shared chrome

- **Title bar**: 46 px tall, full width. Background is a vertical ordered
  (Bayer 4×4) dither from `#AAAAAA` at the top to white at the bottom; 2 px
  black rule underneath. Title centred, `row`-ish 22 px 600. Left: ▶ glyph
  (play) or a lock glyph when asleep. Right: battery 32 × 15, and the clock
  (`sm`) to its left when there is room.
- **List row**: 56 px tall, label at x = 16 vertically centred, chevron `>`
  7 px / 3 px stroke at x = right − 22.
- **Selection**: the whole row filled black, text white. No grey lip.
- **Split screen** (Home): menu column 0–286 px, 2 px black divider, art pane
  286–528 px. The art pane changes with the highlighted row.
- **Scrubber**: 2 px outlined bar with a black fill and a diamond marker;
  labels in `xs` below.
- **Pips** (offices kept): four 7 px squares, 4 px apart; filled = kept,
  1 px outline = not kept. Never drawn for future days.
- **Feast / fast marks**: a small five-point star (feast) and a fish outline
  (Friday devotion), drawn in the cell's top-right corner.

## Screens

| # | Screen | Render | Notes |
| --- | --- | --- | --- |
| 1 | Sleep ("Now Praying") | `screens/lock.png` | Today's cover 300 × 450, centred; next office + hour; commemoration in grey `xs`; four-segment day scrubber labelled 8:30 · 12:00 · 4:30 · 8:00. Rendered once at sleep. |
| 2 | Home | `screens/home.png` | Rows: Today's Office · Calendar · Lectionary · Library · Book Clubs · Shuffle Psalms · Settings. Art pane: cover 214 × 321, date, Proper, four office hours with ✓ kept / ◀ due, commemoration. "Continue reading" strip under the menu. |
| 3 | Today's Office | `screens/today.png` | Header block: cover 120 × 180 + date, Proper, commemoration, pips. Four 104 px rows (number, name, hour, psalms, lessons, status). The office the clock picks is selected on entry. |
| 4 | Calendar | `screens/calendar.png` | 7 × 5 grid, cells 72 × 74 from y = 92. Today inverted. Pips per day; star / fish marks. Month totals below: offices kept, full days, streak, Fridays kept. |
| 5 | Lectionary | `screens/lectionary.png` | Day strip (6 days, ◀ goes back). Selected day: Proper, year, commemoration, then rows for MP psalms, lessons, EP psalms, gospel, collect. Black bar at the foot: open that day's EPUB. |
| 6 | Reader | `screens/reader.png` | Existing reader. Rotunda only adds the title bar (passage + "1 of 3") and the minutes-left scrubber at the foot. |
| 7 | Library | `screens/library.png` | Flat Cover Flow: centre cover 236 × 354 at y = 96; three skewed covers each side, pre-skewed and cached once at import. Title / author / shelf / progress under it; A–Z jump strip. Black background. |
| 8 | Book Clubs | `screens/clubs.png` | Up to four 150 px rows: cover 90 × 135, club name, assignment, pages left + meeting day, progress bar, notes file. Empty fourth slot shows "New Club". |

## Office hours and "current office"

Fixed hours, editable in `/office/settings.tsv`:
Morning 8:30 · Noonday 12:00 · Evening 4:30 pm · Compline 8:00 pm.

The current office comes from the RTC (the X3 has a DS3231):

| Clock | Current office |
| --- | --- |
| 04:00–11:59 | Morning Prayer |
| 12:00–16:29 | Noonday Prayer |
| 16:30–19:59 | Evening Prayer |
| 20:00–03:59 | Compline (counts toward the day it began) |

If the RTC is unavailable, fall back to Morning and show no clock.

## Buttons

| X3 key | Rotunda meaning |
| --- | --- |
| ◀ / ▶ (page keys) | scroll the list (the "wheel") |
| OK | select / open |
| BACK | up one level; hold = Home |
| Tilt | page turn in the reader only |
