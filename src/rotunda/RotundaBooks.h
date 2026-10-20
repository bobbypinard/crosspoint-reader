#pragma once

#include <OfficeTypes.h>

#include <cstddef>
#include <string>

// Book-level helpers for Rotunda screens: opening an EPUB at a TOC entry and
// reading the reader's saved position without opening the reader.
namespace RotundaBooks {

// One-shot request consumed by EpubReaderActivity::loadBook() for this path:
// open at the TOC entry with this title (or index, when title is empty).
struct OpenRequest {
  char title[48] = "";
  int tocIndex = -1;
};
bool takeOpenRequest(const char* path, OpenRequest& out);
// TOC title equality ignoring surrounding whitespace.
bool tocTitleMatches(const std::string& tocTitle, const char* wanted);

// Opens the reader at the office's chapter (DATA.md TOC titles).
void openOffice(const char* path, rotunda::Office office);
// Opens the reader at a 0-based TOC index.
void openAtTocIndex(const char* path, int tocIndex);
// Plain open at the saved position.
void open(const char* path);

struct Position {
  int chapter = 0;  // 1-based TOC entry of the saved position
  int percent = 0;  // whole-book progress
  int spine = 0;
  int page = 0;
  int pageCount = 0;  // pages in the saved chapter, 0 if unknown
};
// Needs the book's metadata cache (built the first time the book was opened).
bool readPosition(const char* path, Position& out);

// Pages from the saved position to the end of TOC entry endToc and progress
// through startToc..endToc (both 1-based). Estimated from item sizes.
bool assignmentProgress(const char* path, int startToc, int endToc, int& pagesLeft, int& permille);

int tocCount(const char* path);

bool isDailyOfficeEpub(const std::string& path);

}  // namespace RotundaBooks
