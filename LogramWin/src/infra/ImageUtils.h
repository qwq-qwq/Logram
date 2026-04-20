#pragma once
#include <windows.h>

// Decode a PNG stored as RCDATA and return a 32bpp top-down DIB section
// pre-scaled to (size × size). Caller owns the HBITMAP (DeleteObject).
// Returns nullptr on failure.
HBITMAP LoadPngResourceAsHBITMAP(HINSTANCE hInstance, int resourceId, int size);