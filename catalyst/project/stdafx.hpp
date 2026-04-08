#pragma once

// macros
#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

// windows
#include <windows.h>
#include <tlhelp32.h>
#include <dwmapi.h>
#include <windowsx.h>
#include <shellapi.h>
#include <shlobj.h>
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

// standard
#include <array>
#include <vector>
#include <cmath>
#include <numbers>
#include <algorithm>
#include <format>
#include <shared_mutex>
#include <unordered_set>
#include <chrono>
#include <deque>

// external
#include <external/zdraw/zui/zui.hpp>
#include <external/poly2d.hpp>
#include <external/xorstr.hpp>

// resources
#include <resources/fonts/mochi.hpp>
#include <resources/fonts/pixel7.hpp>
#include <resources/fonts/pretzel.hpp>
#include <resources/fonts/weapons.hpp>

// utilities
#include <utilities/math/math.hpp>
#include <utilities/animation/animation.hpp>
#include <utilities/console/console.hpp>
#include <utilities/input/input.hpp>
#include <utilities/input/csgoinput.hpp>
#include <utilities/memory/memory.hpp>
#include <utilities/modules/modules.hpp>
#include <utilities/offsets/offsets.hpp>
#include <utilities/cstypes.hpp>
#include <utilities/fnv1a.hpp>
#include <utilities/random.hpp>

// core
#include <core/settings.hpp>
#include <core/config/config.hpp>
#include <core/systems/systems.hpp>
#include <core/render/render.hpp>
#include <core/threads/threads.hpp>
#include <core/features/features.hpp>
#include <core/menu/menu.hpp>

namespace g {

	inline console console{};
	inline input input{};
	inline csgoinput csgo_input{};
	inline memory memory{};
	inline modules modules{};
	inline offsets offsets{};

	inline render render{};
	inline menu menu{};

} // namespace g