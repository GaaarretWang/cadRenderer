#pragma once
#include <json.hpp>

class GlobalInfo {
public:
	using Json = nlohmann::json;
public:
	GlobalInfo();
	static GlobalInfo& get();
public:
	Json initParams;
};

