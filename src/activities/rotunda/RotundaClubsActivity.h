#pragma once

#include <Clubs.h>

#include "RotundaScreen.h"

// Book Clubs (screens/clubs.png) from /clubs/clubs.tsv: up to four clubs with
// cover, assignment, pages left and meeting day; an empty "New Club" slot
// fills the rest. OK opens the book at its saved position, holding OK opens
// the notes file.
class RotundaClubsActivity final : public RotundaScreen {
 public:
  RotundaClubsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : RotundaScreen("RotundaClubs", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  struct ClubStatus {
    int pagesLeft = -1;  // -1 = unknown
    int permille = 0;
    char notesTitle[64] = "";
  };

  void renderClub(int index, int y);
  void renderEmptySlot(int y);

  rotunda::ClubList clubs;
  ClubStatus status[rotunda::ClubList::MAX_CLUBS];
  int selected = 0;
};
