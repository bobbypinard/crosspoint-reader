#pragma once

#include <string>

// Pre-skewed side covers for the Library's flat Cover Flow
// (docs/rotunda/DESIGN.md screen 7). Each book's 1-bit thumbnail is squeezed
// into a perspective strip once and cached next to its other cover files as
// flow_l.bmp / flow_r.bmp; drawing never transforms covers.
namespace CoverFlowCache {

constexpr int CENTER_W = 236;
constexpr int CENTER_H = 354;
constexpr int SIDE_W = 84;
constexpr int SIDE_OUTER_H = 290;  // height at the edge away from the centre

// Path of the cached strip, generating it first when missing. leftSide: the
// strip's outer (short) edge is on the left. Empty when the book has no cover.
std::string sidePath(const std::string& bookPath, bool leftSide);

// The flat centre cover thumbnail, generated when missing.
std::string centerPath(const std::string& bookPath);

}  // namespace CoverFlowCache
