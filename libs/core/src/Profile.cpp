#include "beam/core/Profile.hpp"

#include <algorithm>
#include <cmath>

namespace beam::core {
namespace {

bool inBounds(const BeamFrame& frame, int x, int y) {
  return x >= 0 && y >= 0 && static_cast<std::size_t>(x) < frame.width &&
         static_cast<std::size_t>(y) < frame.height;
}

}  // namespace

LineProfile extractHorizontalProfile(const BeamFrame& frame, int row) {
  LineProfile out;
  out.axis = ProfileAxis::Horizontal;
  out.row_or_col = row;
  out.y0 = out.y1 = row;
  if (frame.empty() || row < 0 || static_cast<std::size_t>(row) >= frame.height) {
    return out;
  }
  out.x0 = 0;
  out.x1 = static_cast<int>(frame.width) - 1;
  out.position_um.reserve(frame.width);
  out.intensity.reserve(frame.width);
  for (std::size_t x = 0; x < frame.width; ++x) {
    out.position_um.push_back((static_cast<double>(x) + 0.5) * frame.pixelSizeX_um);
    out.intensity.push_back(frame.intensity[static_cast<std::size_t>(row) * frame.width + x]);
  }
  out.valid = true;
  return out;
}

LineProfile extractVerticalProfile(const BeamFrame& frame, int col) {
  LineProfile out;
  out.axis = ProfileAxis::Vertical;
  out.row_or_col = col;
  out.x0 = out.x1 = col;
  if (frame.empty() || col < 0 || static_cast<std::size_t>(col) >= frame.width) {
    return out;
  }
  out.y0 = 0;
  out.y1 = static_cast<int>(frame.height) - 1;
  out.position_um.reserve(frame.height);
  out.intensity.reserve(frame.height);
  for (std::size_t y = 0; y < frame.height; ++y) {
    out.position_um.push_back((static_cast<double>(y) + 0.5) * frame.pixelSizeY_um);
    out.intensity.push_back(frame.intensity[y * frame.width + static_cast<std::size_t>(col)]);
  }
  out.valid = true;
  return out;
}

LineProfile extractLineProfile(const BeamFrame& frame, int x0, int y0, int x1, int y1) {
  LineProfile out;
  out.axis = ProfileAxis::Arbitrary;
  out.x0 = x0;
  out.y0 = y0;
  out.x1 = x1;
  out.y1 = y1;
  if (frame.empty() || !inBounds(frame, x0, y0) || !inBounds(frame, x1, y1)) {
    return out;
  }

  // Bresenham
  int x = x0;
  int y = y0;
  const int dx = std::abs(x1 - x0);
  const int dy = -std::abs(y1 - y0);
  const int sx = x0 < x1 ? 1 : -1;
  const int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;

  double dist = 0.0;
  int prevX = x0;
  int prevY = y0;

  auto push = [&](int px, int py) {
    if (px != prevX || py != prevY) {
      const double ddx = (px - prevX) * frame.pixelSizeX_um;
      const double ddy = (py - prevY) * frame.pixelSizeY_um;
      dist += std::sqrt(ddx * ddx + ddy * ddy);
      prevX = px;
      prevY = py;
    }
    out.position_um.push_back(dist);
    out.intensity.push_back(
        frame.intensity[static_cast<std::size_t>(py) * frame.width +
                        static_cast<std::size_t>(px)]);
  };

  while (true) {
    push(x, y);
    if (x == x1 && y == y1) {
      break;
    }
    const int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y += sy;
    }
  }

  out.valid = !out.intensity.empty();
  return out;
}

CrosshairProfiles extractCrosshair(const BeamFrame& frame, int x, int y) {
  CrosshairProfiles out;
  out.horizontal = extractHorizontalProfile(frame, y);
  out.vertical = extractVerticalProfile(frame, x);
  return out;
}

}  // namespace beam::core
