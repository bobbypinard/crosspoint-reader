#pragma once

#include <HalDisplay.h>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

// Shared behaviour of the Rotunda screens (DESIGN.md "Buttons"): a clean
// refresh when the screen first appears, fast 1-bit refreshes for focus moves,
// and BACK up one level (to Home unless the screen was pushed).
class RotundaScreen : public Activity {
 public:
  RotundaScreen(std::string name, GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity(std::move(name), renderer, mappedInput) {}

 protected:
  // Pushes the framebuffer: HALF on the first frame (clears ghosting from the
  // previous screen and its covers), FAST for every later focus move.
  void present() {
    renderer.displayBuffer(firstFrameShown ? HalDisplay::FAST_REFRESH : HalDisplay::HALF_REFRESH);
    firstFrameShown = true;
  }
  // Forces the next present() to be a clean refresh.
  void markFullRefresh() { firstFrameShown = false; }

  // True when BACK was released this frame; the screen should leave.
  bool backReleased() const { return mappedInput.wasReleased(MappedInputManager::Button::Back); }
  bool confirmReleased() const { return mappedInput.wasReleased(MappedInputManager::Button::Confirm); }
  bool confirmLongPressed() const {
    return mappedInput.wasLongPressed(MappedInputManager::Button::Confirm, LONG_PRESS_MS);
  }

  static constexpr unsigned long LONG_PRESS_MS = 700;
  ButtonNavigator buttonNavigator;

 private:
  bool firstFrameShown = false;
};
