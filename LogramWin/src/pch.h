#pragma once

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <dwmapi.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <shobjidl.h>
#include <shellapi.h>
#endif

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <unordered_map>
#include <set>
#include <algorithm>
#include <functional>
#include <atomic>
#include <thread>
#include <memory>
#include <cassert>
