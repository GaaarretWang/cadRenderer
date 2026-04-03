#pragma once

#ifdef CAD_DATA_MANAGER_SHARED_BUILD
#ifdef _WIN32
#ifdef CAD_DATA_MANAGER_EXPORTS
#define CAD_DATA_MANAGER_API __declspec(dllexport)
#else
#define CAD_DATA_MANAGER_API __declspec(dllimport)
#endif // EXPORT
#else
#define CAD_DATA_MANAGER_API
#endif // WIN32
#else
#define CAD_DATA_MANAGER_API
#endif //CAD_DATA_MANAGER_SHARED_BUILD

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <list>
#include <queue>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <regex>
#include <sstream>
#include <limits>
#include <utility>
#include <memory>
#include <spdlog/spdlog.h>

constexpr auto mPI = 3.1415926f;
constexpr auto MAX1 = std::numeric_limits<int>::max();