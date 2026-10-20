#!/usr/bin/env python3
"""Build Rotunda's /office/index/YYYY-MM.tsv files from the Daily Office EPUBs on an SD card.

Usage: python3 scripts/rotunda_build_index.py /path/to/sd-card

Scans the card for daily-office-1979-rite1-YYYY-MM-DD.epub, reads each book's
title card and office pages, and writes one row per day as described in
docs/rotunda/DATA.md. Books in /office/books win over copies elsewhere (such as
/read). Existing index files for the months found are replaced.

The EPUBs carry no lesser commemorations, so that column stays empty.
"""

import html
import os
import re
import sys
import zipfile

NAME = re.compile(r"^daily-office-1979-rite1-(\d{4})-(\d{2})-(\d{2})\.epub$")
COLUMNS = ["date", "weekday", "season", "proper", "year", "rank", "feast", "commemoration",
           "mp_psalms", "ep_psalms", "mp_lessons", "ep_lessons", "collect", "epub"]
SKIP_DIRS = {".crosspoint", "XTCache", "System Volume Information"}


def text(fragment):
    return html.unescape(re.sub(r"<[^>]+>", "", fragment)).strip()


def classed(markup, cls):
    return [text(m) for m in re.findall(r'class="%s"[^>]*>(.*?)</(?:p|h\d)>' % cls, markup, re.S)]


def after_dash(value):
    return value.split("—", 1)[1].strip() if "—" in value else value.strip()


def office(markup):
    psalms = [p[len("Psalm "):] for p in classed(markup, "ref") if p.startswith("Psalm ")]
    lessons = [after_dash(h) for h in classed(markup, "ch")]
    collect = next((after_dash(c) for c in classed(markup, "cl") if c.startswith("The Collect of the Day")), "")
    return " ".join(psalms), "; ".join(lessons), collect


def read_day(path):
    with zipfile.ZipFile(path) as z:
        names = {os.path.basename(n): n for n in z.namelist()}
        card = z.read(names["date.xhtml"]).decode("utf-8")
        mp = z.read(names["mp.xhtml"]).decode("utf-8")
        ep = z.read(names["ep.xhtml"]).decode("utf-8")
    title = re.search(r'class="tc-title">(.*?)</h1>', card, re.S).group(1)
    weekday = text(title.split("<br")[0])
    sub_lines = [text(s) for s in re.search(r'class="tc-sub">(.*?)</p>', card, re.S).group(1).split("<br/>")]
    feast = "" if sub_lines[0].startswith("Week of ") else sub_lines[0]
    week_line = next((s for s in sub_lines if "Year " in s), "")
    year = re.search(r"Year \w+", week_line).group(0) if "Year " in week_line else ""
    proper = re.search(r"Proper \d+", week_line).group(0) if "Proper " in week_line else ""
    mp_psalms, mp_lessons, collect = office(mp)
    ep_psalms, ep_lessons, ep_collect = office(ep)
    return {
        "weekday": weekday, "proper": proper, "year": year, "feast": feast,
        "mp_psalms": mp_psalms, "ep_psalms": ep_psalms, "mp_lessons": mp_lessons, "ep_lessons": ep_lessons,
        "collect": collect or ep_collect,
    }


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    root = sys.argv[1].rstrip("/")
    found = {}
    for folder, dirs, files in os.walk(root):
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS and not d.startswith(".")]
        for f in files:
            m = NAME.match(f)
            if not m:
                continue
            card_path = "/" + os.path.relpath(os.path.join(folder, f), root)
            date = "-".join(m.groups())
            preferred = card_path.startswith("/office/books/")
            if date not in found or preferred:
                found[date] = card_path

    days = {}
    for date, card_path in sorted(found.items()):
        try:
            days[date] = read_day(root + card_path)
            days[date]["epub"] = card_path
        except (KeyError, AttributeError, zipfile.BadZipFile) as e:
            print("skipped %s: %s" % (card_path, e))

    # Feast days print no Proper; they keep the week's, carried from the nearest day before or after.
    ordered = sorted(days)
    for i, date in enumerate(ordered):
        if days[date]["proper"]:
            continue
        neighbours = ordered[i - 1::-1] + ordered[i + 1:] if i > 0 else ordered[i + 1:]
        days[date]["proper"] = next((days[d]["proper"] for d in neighbours if days[d]["proper"]), "")

    months = {}
    for date in ordered:
        d = days[date]
        d["date"] = date
        d["season"] = "Season after Pentecost" if d["proper"].startswith("Proper") else ""
        d["rank"] = ("feast" if d["feast"] else "sunday" if d["weekday"] == "Sunday"
                     else "fast" if d["weekday"] == "Friday" else "ferial")
        d["commemoration"] = ""
        months.setdefault(date[:7], []).append(d)

    os.makedirs(os.path.join(root, "office", "index"), exist_ok=True)
    for month, rows in sorted(months.items()):
        out = os.path.join(root, "office", "index", month + ".tsv")
        with open(out, "w", encoding="utf-8", newline="\n") as f:
            f.write("\t".join(COLUMNS) + "\n")
            for row in rows:
                f.write("\t".join(row[c].replace("\t", " ") for c in COLUMNS) + "\n")
        print("wrote %s (%d days)" % (os.path.relpath(out, root), len(rows)))


if __name__ == "__main__":
    main()
