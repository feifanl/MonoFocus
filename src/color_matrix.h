#pragma once
#include <windows.h>
#include <magnification.h>

// MAGCOLOREFFECT is a 5x5 row-major matrix, row-vector convention ([r g b a 1] x M).
// s: 0 = full grayscale, 1 = identity (no effect). Grayscale weights are Rec. 709
// luma. One formula serves both Mode A and Mode B (PLAN §2, verbatim).

inline MAGCOLOREFFECT SaturationMatrix(float s) {   // s: 0 = gray, 1 = identity
    const float lr = 0.2126f, lg = 0.7152f, lb = 0.0722f;
    const float t = 1.0f - s;
    MAGCOLOREFFECT m = {{
        { t*lr + s, t*lr,     t*lr,     0, 0 },
        { t*lg,     t*lg + s, t*lg,     0, 0 },
        { t*lb,     t*lb,     t*lb + s, 0, 0 },
        { 0,        0,        0,        1, 0 },
        { 0,        0,        0,        0, 1 },
    }};
    return m;
}
