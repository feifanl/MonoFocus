#include "effect_fullscreen.h"

#include <windows.h>
#include <magnification.h>

#include "color_matrix.h"

namespace FullscreenEffect {

void Set(float saturation) {
    MAGCOLOREFFECT m = SaturationMatrix(saturation);
    MagSetFullscreenColorEffect(&m);
}

void Clear() {
    MAGCOLOREFFECT m = SaturationMatrix(1.0f);   // identity
    MagSetFullscreenColorEffect(&m);
}

}  // namespace FullscreenEffect
