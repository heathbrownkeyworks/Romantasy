#pragma once

#pragma warning(push)
#include <RE/Skyrim.h>
#include <REL/Relocation.h>
#include <SKSE/SKSE.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <deque>
#include <format>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#ifdef NDEBUG
#	include <spdlog/sinks/basic_file_sink.h>
#else
#	include <spdlog/sinks/msvc_sink.h>
#endif
#pragma warning(pop)

using namespace std::literals;

namespace logger = SKSE::log;

namespace util
{
    using SKSE::stl::report_and_fail;
}

#define DLLEXPORT __declspec(dllexport)

#include "plugin.h"
