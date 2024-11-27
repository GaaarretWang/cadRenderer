#pragma once
#include "renderInstance.h"
#include <unordered_map>

class RenderInstanceManager
{
private:
	std::unordered_map<std::string, RInstance> mRInstanceMap;
	RenderInstanceManager() {};

public:
	static RenderInstanceManager& GetInstance() {
		static RenderInstanceManager instance;
		return instance;
	}

	void addRInstance(RInstance ins);

	void removeRInstance(std::string id);

	bool existRInstance(std::string id);

	void clearRInstance();

	void addToRInstanceMap(RInstance ins);

	std::unordered_map<std::string, RInstance> getRInstanceMap() { return mRInstanceMap; };
};