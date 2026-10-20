#include "RotundaTheme.h"

#include <GfxRenderer.h>

#include <string>

#include "RotundaChrome.h"
#include "fontIds.h"

void RotundaTheme::drawHeader(const GfxRenderer& renderer, const Rect rect, const char* title, const char* subtitle,
                              const bool /*backButton*/) const {
  // Stock screens pass a band starting at topPadding (0 here); the bar is
  // always the 46 px Rotunda title bar at the top of the screen.
  (void)rect;
  RotundaChrome::TitleBar bar;
  bar.title = title;
  bar.rightLabel = subtitle;
  bar.titleFont = UI_12_FONT_ID;
  bar.titleBold = true;
  RotundaChrome::drawTitleBar(renderer, bar);
}

void RotundaTheme::fillBatteryIcon(const GfxRenderer& renderer, const Rect rect, const uint16_t percentage) const {
  // Reader status bar battery: same fill rule as the title-bar battery.
  const int inner = rect.width - 5;
  const int fill = inner * (percentage > 100 ? 100 : percentage) / 100;
  if (fill > 0) renderer.fillRect(rect.x + 2, rect.y + 2, fill, rect.height - 4);
}

int RotundaTheme::getMenuRowHeight(const GfxRenderer&) const { return RotundaChrome::ROW_HEIGHT; }

void RotundaTheme::drawButtonMenu(GfxRenderer& renderer, const Rect rect, const int buttonCount,
                                  const int selectedIndex, const std::function<std::string(int index)>& buttonLabel,
                                  const std::function<UIIcon(int index)>& /*rowIcon*/) const {
  for (int i = 0; i < buttonCount; ++i) {
    const int y = rect.y + i * RotundaChrome::ROW_HEIGHT;
    if (y + RotundaChrome::ROW_HEIGHT > rect.y + rect.height) break;
    const std::string label = buttonLabel(i);
    RotundaChrome::drawRow(renderer, rect.x, y, rect.width, RotundaChrome::ROW_HEIGHT, label.c_str(),
                           i == selectedIndex, true, UI_12_FONT_ID);
  }
}
