#pragma once

// Mode A: GPU color matrix on the final DWM output of all monitors via
// MagSetFullscreenColorEffect. Stateless wrapper (PLAN §5).

namespace FullscreenEffect {

// Applies the saturation matrix to the whole screen.
void Set(float saturation);

// Restores identity (visually no effect) — the "off" matrix.
void Clear();

}  // namespace FullscreenEffect
