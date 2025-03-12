#pragma once
#include <json.hpp>
namespace cadDataManager {
	class GlobalInfo {
	public:
		using Json = nlohmann::json;
	public:
		GlobalInfo();
		static GlobalInfo& get();
	public:
		Json initParams;
	};
}
