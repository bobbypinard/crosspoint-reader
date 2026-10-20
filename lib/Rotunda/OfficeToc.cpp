#include "OfficeToc.h"

namespace rotunda {

void OfficeToc::add(const char* title, const int spine, const uint8_t level, const bool atTop) {
  if (tocCount >= MAX_TOC) return;
  Office named;
  const bool heading = officeFromTocTitle(title, named);
  if (heading) {
    currentOffice = static_cast<int>(named);
    officeLevel = level;
  } else if (currentOffice >= 0 && level <= officeLevel) {
    currentOffice = -1;  // a sibling of the offices that is not one of them
  }
  entries[tocCount++] = {static_cast<int16_t>(spine), static_cast<int8_t>(currentOffice), level, atTop, heading};
}

void OfficeToc::resetBounds(const int spine) {
  boundSpine = spine;
  boundCount = 0;
  for (int i = 0; i < tocCount && boundCount < MAX_BOUNDS; ++i) {
    if (entries[i].spine == spine && entries[i].atTop) bounds[boundCount++] = {static_cast<int16_t>(i), 0};
  }
}

void OfficeToc::addBound(const int toc, const int page) {
  if (boundCount >= MAX_BOUNDS || toc < 0 || toc >= tocCount || page < 0) return;
  bounds[boundCount++] = {static_cast<int16_t>(toc), static_cast<uint16_t>(page)};
}

int OfficeToc::tocAtSpineStart(const int spine) const {
  int found = -1;
  for (int i = 0; i < tocCount; ++i) {
    if (entries[i].spine < spine || (entries[i].spine == spine && entries[i].atTop)) found = i;
  }
  return found;
}

int OfficeToc::tocAtPage(const int spine, const int page) const {
  int found = tocAtSpineStart(spine);
  if (spine != boundSpine) return found;
  for (int i = 0; i < boundCount; ++i) {
    if (bounds[i].page <= page && bounds[i].toc > found) found = bounds[i].toc;
  }
  return found;
}

bool OfficeToc::isOfficeEnd(const int spine, const int page, const int pageCount, const bool layoutComplete,
                            const int spineCount) const {
  const int here = officeAtPage(spine, page);
  if (here < 0) return false;
  if (page + 1 < pageCount) return officeAtPage(spine, page + 1) != here;
  if (!layoutComplete) return false;  // more pages may follow in this item
  const int nextSpine = spine + 1;
  return nextSpine >= spineCount || office(tocAtSpineStart(nextSpine)) != here;
}

void OfficeToc::siblingPosition(const int toc, int& index, int& total) const {
  index = 0;
  total = 0;
  const int officeId = office(toc);
  if (officeId < 0 || entries[toc].heading) return;
  for (int i = 0; i < tocCount; ++i) {
    if (entries[i].office != officeId || entries[i].level != entries[toc].level) continue;
    ++total;
    if (i == toc) index = total;
  }
}

}  // namespace rotunda
