#pragma once

#include <cstdint>

#include "OfficeTypes.h"

namespace rotunda {

// Which office each TOC entry of a Daily Office EPUB belongs to, and where the
// entries start inside the current spine item. Pure data, so the "office kept"
// rule (DATA.md) is host-testable without an EPUB or layout engine.
class OfficeToc {
 public:
  static constexpr int MAX_TOC = 96;
  static constexpr int MAX_BOUNDS = 24;

  // Feed TOC entries in book order. atTop: no anchor, so the entry starts at
  // the top of its spine item. Entries past MAX_TOC are ignored.
  void add(const char* title, int spine, uint8_t level, bool atTop);
  int count() const { return tocCount; }
  int office(int toc) const { return toc >= 0 && toc < tocCount ? entries[toc].office : -1; }
  int spine(int toc) const { return toc >= 0 && toc < tocCount ? entries[toc].spine : -1; }
  bool atTop(int toc) const { return toc >= 0 && toc < tocCount && entries[toc].atTop; }

  // Start pages of the TOC entries inside one spine item. Anchors not laid out
  // yet are simply not added.
  void resetBounds(int spine);
  void addBound(int toc, int page);
  int boundsSpine() const { return boundSpine; }

  // TOC entry in force at a page / at the top of a spine item; -1 before the first.
  int tocAtPage(int spine, int page) const;
  int tocAtSpineStart(int spine) const;

  // The office shown on this page, -1 outside the offices.
  int officeAtPage(int spine, int page) const { return office(tocAtPage(spine, page)); }

  // True when this page is the last page of its office: the next page belongs
  // to another office, or this is the end of a fully laid-out spine item and
  // the next item starts elsewhere (or the book ends).
  bool isOfficeEnd(int spine, int page, int pageCount, bool layoutComplete, int spineCount) const;

  // Position of an entry among same-level entries of its office (1-based);
  // total 0 for the office heading itself or an entry outside the offices.
  void siblingPosition(int toc, int& index, int& total) const;

 private:
  struct Entry {
    int16_t spine;
    int8_t office;
    uint8_t level;
    bool atTop;
    bool heading;  // the office's own entry ("Morning Prayer")
  };
  struct Bound {
    int16_t toc;
    uint16_t page;
  };

  Entry entries[MAX_TOC] = {};
  int tocCount = 0;
  int currentOffice = -1;
  uint8_t officeLevel = 0;
  Bound bounds[MAX_BOUNDS] = {};
  int boundCount = 0;
  int boundSpine = -1;
};

}  // namespace rotunda
