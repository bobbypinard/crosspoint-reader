#include "RotundaBooks.h"

#include <Epub.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>
#include <OfficeIndex.h>

#include <algorithm>
#include <cctype>
#include <cstring>

#include "activities/Activity.h"

namespace RotundaBooks {

namespace {
// Pending open request; static so it costs no heap and survives the activity swap.
char requestPath[160] = "";
OpenRequest pending;

// Fallback when the saved chapter has no page count yet.
constexpr size_t DEFAULT_BYTES_PER_PAGE = 2200;

struct Progress {
  int spine = 0;
  int page = 0;
  int pageCount = 0;
};

bool readProgressFile(const Epub& epub, Progress& out) {
  HalFile f;
  if (!Storage.openFileForRead("ROT", epub.getCachePath() + "/progress.bin", f)) return false;
  uint8_t data[6] = {};
  const int n = f.read(data, sizeof(data));
  if (n < 4) return false;
  out.spine = data[0] | (data[1] << 8);
  out.page = data[2] | (data[3] << 8);
  if (out.page == 0xFFFF) out.page = 0;
  out.pageCount = n >= 6 ? (data[4] | (data[5] << 8)) : 0;
  return true;
}

// Loads the metadata cache only; never builds it from the EPUB.
std::unique_ptr<Epub> loadCached(const char* path) {
  if (!path || !*path || !Storage.exists(path)) return nullptr;
  auto epub = makeUniqueNoThrow<Epub>(path, "/.crosspoint");
  if (!epub) {
    LOG_ERR("ROT", "OOM: EPUB for position");
    return nullptr;
  }
  if (!epub->load(false, true)) return nullptr;
  return epub;
}

size_t spineStart(const Epub& epub, const int spine) {
  return spine >= 1 ? epub.getCumulativeSpineItemSize(spine - 1) : 0;
}

void request(const char* path, const char* title, const int tocIndex) {
  rotunda::copyField(requestPath, sizeof(requestPath), path);
  rotunda::copyField(pending.title, sizeof(pending.title), title);
  pending.tocIndex = tocIndex;
}
}  // namespace

bool takeOpenRequest(const char* path, OpenRequest& out) {
  if (!requestPath[0] || !path || strcmp(path, requestPath) != 0) return false;
  out = pending;
  requestPath[0] = '\0';
  pending = OpenRequest{};
  return true;
}

bool tocTitleMatches(const std::string& tocTitle, const char* wanted) {
  size_t start = 0;
  size_t end = tocTitle.size();
  while (start < end && isspace(static_cast<unsigned char>(tocTitle[start]))) ++start;
  while (end > start && isspace(static_cast<unsigned char>(tocTitle[end - 1]))) --end;
  const size_t n = strlen(wanted);
  return end - start == n && tocTitle.compare(start, n, wanted) == 0;
}

void openOffice(const char* path, const rotunda::Office office) {
  request(path, rotunda::officeTocTitle(office), -1);
  activityManager.goToReader(path);
}

void openAtTocIndex(const char* path, const int tocIndex) {
  request(path, "", tocIndex);
  activityManager.goToReader(path);
}

void open(const char* path) {
  requestPath[0] = '\0';
  activityManager.goToReader(path);
}

bool readPosition(const char* path, Position& out) {
  out = Position{};
  auto epub = loadCached(path);
  if (!epub) return false;
  Progress p;
  if (!readProgressFile(*epub, p)) return false;
  out.spine = p.spine;
  out.page = p.page;
  out.pageCount = p.pageCount;
  const int toc = epub->getTocIndexForSpineIndex(p.spine);
  out.chapter = toc >= 0 ? toc + 1 : 0;
  const float fraction = p.pageCount > 0 ? static_cast<float>(p.page) / static_cast<float>(p.pageCount) : 0.0f;
  out.percent = static_cast<int>(std::clamp(epub->calculateProgress(p.spine, fraction), 0.0f, 1.0f) * 100.0f + 0.5f);
  return true;
}

bool assignmentProgress(const char* path, const int startToc, const int endToc, int& pagesLeft, int& permille) {
  pagesLeft = -1;
  permille = 0;
  if (endToc <= 0) return false;
  auto epub = loadCached(path);
  if (!epub) return false;
  const int tocTotal = epub->getTocItemsCount();
  const int spineTotal = epub->getSpineItemsCount();
  if (tocTotal <= 0 || spineTotal <= 0 || endToc > tocTotal) return false;

  const int startSpine = startToc > 0 ? epub->getSpineIndexForTocIndex(startToc - 1) : 0;
  // The assignment ends where the next TOC entry starts (or at the end of the book).
  const int endSpineExclusive = endToc < tocTotal ? epub->getSpineIndexForTocIndex(endToc) : spineTotal;
  const size_t startBytes = spineStart(*epub, startSpine);
  const size_t endBytes = spineStart(*epub, std::max(endSpineExclusive, startSpine + 1));

  Progress p;
  const bool hasProgress = readProgressFile(*epub, p);
  const size_t chapterBytes = spineStart(*epub, p.spine + 1) - spineStart(*epub, p.spine);
  size_t position = startBytes;
  size_t bytesPerPage = DEFAULT_BYTES_PER_PAGE;
  if (hasProgress) {
    position = spineStart(*epub, p.spine);
    if (p.pageCount > 0) {
      position += chapterBytes * static_cast<size_t>(p.page) / static_cast<size_t>(p.pageCount);
      bytesPerPage = std::max<size_t>(1, chapterBytes / static_cast<size_t>(p.pageCount));
    }
  }

  const size_t span = endBytes > startBytes ? endBytes - startBytes : 1;
  const size_t done = position > startBytes ? std::min(position - startBytes, span) : 0;
  permille = static_cast<int>(done * 1000 / span);
  pagesLeft = position >= endBytes ? 0 : static_cast<int>((endBytes - position + bytesPerPage - 1) / bytesPerPage);
  return true;
}

int tocCount(const char* path) {
  auto epub = loadCached(path);
  return epub ? epub->getTocItemsCount() : 0;
}

bool isDailyOfficeEpub(const std::string& path) {
  rotunda::Date d;
  return rotunda::parseOfficeEpubDate(path.c_str(), d);
}

}  // namespace RotundaBooks
