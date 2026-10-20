#pragma once

#include <LibraryIndexFile.h>

#include <memory>
#include <string>

#include "RotundaScreen.h"

// Library as a flat Cover Flow (screens/library.png): centre cover flat, three
// pre-skewed covers each side, title/author/shelf/progress and an A-Z strip.
// Front ◀/▶ flip one book, the side keys jump between letters, OK opens,
// holding OK shows details. Books stream from /.crosspoint/library.idx; only
// the visible seven are read per frame.
class RotundaLibraryActivity final : public RotundaScreen {
 public:
  RotundaLibraryActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : RotundaScreen("RotundaLibrary", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  static constexpr int LETTERS = 27;  // '#' + A-Z
  static constexpr int SIDE_COUNT = 3;

  bool openIndex();
  bool bookAt(int row, std::string& path, std::string* title = nullptr, std::string* author = nullptr);
  static int letterSlot(const std::string& title);
  void jumpLetter(int direction);
  void showDetails();
  void shelfLine(const std::string& path, char* out, size_t cap) const;

  struct IndexReader {
    library::LibraryIndexFile index;
    library::ClixRecord record;
  };
  std::unique_ptr<IndexReader> reader;
  int count = 0;
  int selected = 0;
  uint32_t letterMask = 0;                // bit per letter slot that has a book
  uint16_t letterFirstRow[LETTERS] = {};  // first title-sorted row per letter
  std::string clubBooks[4];
  bool firstFrame = true;
};
