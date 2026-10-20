# Rotunda data contract (SD card)

All Rotunda data is plain **tab-separated** text on the SD card, one header row,
UTF-8, `\n` line endings, no quoting. The EPUB generator writes the office files;
the firmware writes only `said.tsv`. A working sample for October 2026 is in
`sd-sample/` — copy its contents to the root of the SD card to test.

```
/office/settings.tsv        office hours (optional; defaults below)
/office/index/YYYY-MM.tsv   one row per day: what was appointed   (generator writes)
/office/said.tsv            one row per office kept               (firmware appends)
/office/books/daily-office-1979-rite1-YYYY-MM-DD.epub              (generator writes)
/office/psalter.epub        optional: the 150 psalms, one TOC entry each (Shuffle Psalms)
/clubs/clubs.tsv            up to four book clubs                 (you edit)
```

## /office/settings.tsv

| key | value |
| --- | --- |
| morning | 08:30 |
| noonday | 12:00 |
| evening | 16:30 |
| compline | 20:00 |
| day_starts | 04:00 |

## /office/index/YYYY-MM.tsv

| column | example | notes |
| --- | --- | --- |
| date | 2026-10-16 | ISO date |
| weekday | Friday | |
| season | Season after Pentecost | |
| proper | Proper 23 | the week's Proper |
| year | Year Two | Daily Office lectionary year |
| rank | fast | `feast`, `sunday`, `fast` (Fridays) or `ferial` |
| feast | | red-letter feast name, if any |
| commemoration | Hugh Latimer, Nicholas Ridley & Thomas Cranmer | lesser feast, if any |
| mp_psalms | 79 80 81 | space-separated; may contain ranges like `119:105–112` |
| ep_psalms | 82 83 84 85 | |
| mp_lessons | Ecclus. 1:1–10, 18–27; Acts 28:1–16 | `;` between lessons |
| ep_lessons | Luke 9:28–36 | |
| collect | Proper 23 | name of the collect of the day |
| epub | /office/books/daily-office-1979-rite1-2026-10-16.epub | path on the card |

The index is the only source for the Calendar marks and the Lectionary archive,
so past days stay browsable after their EPUBs are deleted.

## /office/said.tsv (firmware appends)

| column | example |
| --- | --- |
| date | 2026-10-16 (the office day, not the wall-clock date — Compline after midnight belongs to the day before) |
| office | `morning`, `noonday`, `evening`, `compline`, or `friday` |
| finished_at | 2026-10-16T08:41 (local time) |
| kind | `office`, or `devotion` for the Friday mark |

An office counts as kept when the reader reaches the last page of that office's
chapter in a Daily Office EPUB. Append one line; never rewrite the file. Ignore
duplicates when reading.

## Daily Office EPUBs

Filename: `daily-office-1979-rite1-YYYY-MM-DD.epub`. The table of contents has
the four offices titled exactly **Morning Prayer**, **Noonday Prayer**,
**Evening Prayer**, **Compline** — match on those titles to open an office and
to detect "kept".

## /clubs/clubs.tsv

| column | example | notes |
| --- | --- | --- |
| slot | 1 | 1–4 |
| name | Bonhoeffer group | |
| book | /Books/The Cost of Discipleship.epub | |
| assignment | Ch. 3–5 | label shown on screen |
| start_toc | 3 | first TOC entry of the assignment (1-based); blank if none |
| end_toc | 5 | last TOC entry of the assignment |
| meets | 2026-10-22T19:00 | next meeting |
| notes | /clubs/bonhoeffer-prep.txt | plain-text prep notes, opened in the reader |

Pages left = pages from the reader's current position to the end of `end_toc`.

## /office/psalter.epub (optional)

Home's **Shuffle Psalms** opens this EPUB at a random table-of-contents entry,
so give it one TOC entry per psalm. Without it, the row explains where to put it.

## Files the firmware caches

Rotunda adds three files to each book's existing cache directory
(`/.crosspoint/epub_<hash>/`), created on first use and safe to delete:

| file | made by | used for |
| --- | --- | --- |
| `flow_l.bmp`, `flow_r.bmp` | Library | pre-skewed side covers (1-bit, 84 × 354) |
| `rotunda_lock.bmp` | sleep screen | today's cover, 2-bit, fitted to 300 × 450 |
