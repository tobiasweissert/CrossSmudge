#pragma once

#include "CrosscubBear120.h"

// drawImage() deliberately does not rotate bitmap bits. Use the renderer's
// oriented pixel path so the editable, upright mascot stays upright in both
// Sticky orientations. This boot/sleep-only blit allocates no buffer.
template <typename Renderer>
inline void drawCrosscubLogo(const Renderer& renderer, int x, int y) {
  for (int row = 0; row < 120; ++row) {
    for (int col = 0; col < 120; ++col) {
      const bool black = (CrosscubBear120[row * 15 + col / 8] & (0x80 >> (col % 8))) == 0;
      renderer.drawPixel(x + col, y + row, black);
    }
  }
}
