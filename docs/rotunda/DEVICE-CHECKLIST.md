# Rotunda device checklist (X3)

Run these on the X3 after flashing a `rotunda` build. Each line says what to do
and what you should see. Compare screens with `docs/rotunda/screens/*.png`.

## Setup

1. Copy the contents of `docs/rotunda/sd-sample/` to the root of the SD card.
2. Add at least one Daily Office EPUB for today to `/office/books/`
   (`daily-office-1979-rite1-YYYY-MM-DD.epub`) with TOC entries titled exactly
   **Morning Prayer**, **Noonday Prayer**, **Evening Prayer**, **Compline**.
3. Put the club books at the paths in `/clubs/clubs.tsv` (or edit the paths).
4. Settings → Clock: sync the clock over WiFi and pick your timezone.
5. Settings → Display → Theme → **Rotunda**. The settings screen redraws at once
   with 56 px rows and black selection; its own header becomes 46 px with a 2 px
   rule (Settings draws its header itself, so it has no dither).
6. Settings → Display → Sleep Screen → **Now Praying**.
7. Serial log: `python3 scripts/debugging_monitor.py` while testing; Rotunda logs
   with the `ROT` tag.

## Every screen

| Screen | Do | Expect |
| --- | --- | --- |
| Home | Press BACK to Home. Step ◀/▶ through all seven rows. | Selection moves with a fast refresh (no full flash). The art pane changes per row: Today = cover, date, Proper, hours with ✓ kept / ◀ due, commemoration. "Continue reading" shows your last non-office book. |
| Home → Shuffle Psalms | OK on the row without `/office/psalter.epub`. | The pane explains where the psalter goes. With the file present, OK opens a random psalm. |
| Today's Office | OK on Today's Office. | The clock's office is selected. Psalms, lessons, kept time and "now" match the index and `said.tsv`. OK opens the EPUB at that office's chapter. |
| Calendar | OK on Calendar. Front ◀/▶ one press each; hold ▶; side keys. | One day per press; holding steps whole months; side keys step weeks. Pips, stars and fish match `screens/calendar.png` for October 2026 with the sample data (55 of 68, 9, 17 days, 3 of 3 on 17 Oct). |
| Calendar → Lectionary | OK on a day, then BACK. | The Lectionary opens on that day; BACK returns to the Calendar, not Home. |
| Lectionary | Front ◀ past the first day in the strip, and across a month boundary. Side keys move focus. | The strip scrolls; days without an index row say so. A reading row opens the EPUB at Morning/Evening Prayer; the black bar opens the whole day; with the EPUB deleted the bar says "EPUB not on card" and nothing opens. |
| Book Clubs | OK on Book Clubs. OK on a club; BACK; hold OK on a club. | Covers, assignment, pages left and meeting day show. OK opens the book where you left it; holding OK opens the notes file. The fourth slot reads "New Club". |
| Library | OK on Library (first visit shows "Loading"). Front ◀/▶; side keys; hold OK. | Centre cover flat, three skewed covers each side, title/author/shelf/progress, A–Z strip with the current letter boxed. Side keys jump letters. Hold OK shows details; Open there opens the book. Second visit: no "Loading", covers come from `flow_l/r.bmp`. |
| Reader chrome | Open an office from Today's Office. | Title bar shows the passage (e.g. "Psalm 86") and "1 of 3"; the foot shows the minutes scrubber. Open a non-office book: the normal status bar is back. Rotate the reader to landscape: Rotunda chrome goes away, the stock bar returns. |
| Sleep | Press power to sleep. | "Now Praying": cover in grey, office due and its hour, commemoration, four-segment scrubber with kept offices solid, "Press power to wake". One grey refresh only. |
| Stock screens | Open the file browser, Clock settings, a reader menu. | Screens that use the theme header get the dithered Rotunda bar; lists get 56 px rows with black selection; text stays in the UI font. Nothing overlaps the 46 px bar. |

## Office windows

There is no manual time setting, so move the office hours instead: edit
`/office/settings.tsv` so a boundary falls a few minutes from now, reopen Today's
Office before and after it, and put the file back afterwards.

| Boundary | settings.tsv | Before | After |
| --- | --- | --- | --- |
| 12:00 | `noonday` = now + 2 min | Morning selected, ◀ on Morning | Noonday selected |
| 16:30 | `evening` = now + 2 min | Noonday | Evening |
| 20:00 | `compline` = now + 2 min | Evening | Compline |
| Midnight | `day_starts` = now + 2 min (after 00:00 local) | Compline of **yesterday** (Home shows yesterday's date and EPUB) | Morning of today |

Also check the real boundaries once with the default hours, if you are awake
for them. Read Compline to its last page after midnight: the new `said.tsv` line
carries yesterday's date in the first column and today's date in `finished_at`.

## Kept offices

1. Read Morning Prayer to its last page. `/office/said.tsv` gains one line
   `YYYY-MM-DD<TAB>morning<TAB>…T…<TAB>office`; Today's Office shows "kept h:mm" and ✓.
2. Page back and forward over the same last page: no second line.
3. On a Friday, hold OK on Today's Office: a `friday … devotion` line, and the
   fish on the Calendar day counts toward "Fridays kept". On other days the
   screen says the devotion is kept on Fridays and nothing is written.

## Failure cases

| Case | Do | Expect |
| --- | --- | --- |
| Missing EPUB | Rename today's EPUB. | Today's Office lists the offices and says the book is missing (with the expected path); OK does nothing; no crash. Home's art pane shows a framed "The Daily Office" placeholder. |
| Missing index | Rename `/office/index/` for this month. | Home/Today/Lectionary say there is no index for the day; Calendar shows pips but no marks. |
| Empty clubs | Leave only the header line in `/clubs/clubs.tsv` (or delete it). | Book Clubs shows "0 + 4" and one "New Club" slot; Home's Book Clubs pane says no meetings are set. |
| No said.tsv | Delete `/office/said.tsv`, read an office to its end. | The file is created with its header line plus one entry. |

## Refresh and memory

1. Settings → Display → Refresh Frequency → 15 pages (there is no 20 option);
   turn 20 pages in an office EPUB. A full refresh happens at page 15, the
   chrome redraws cleanly, no ghosting builds up.
2. In the serial log, note free heap on Home, then open and leave each Rotunda
   screen and an office EPUB three times. Free heap returns to the same value
   (no leak) and stays above 50 KB.
3. Change Theme back to Lyra: the stock Home returns; switch back to Rotunda.
