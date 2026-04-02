#pragma once
#include "model/instance/minstance.h"
namespace cadDataManager {
	class InstanceAppearanceUtil
	{
	public:
		static bool isRenderNormal(Instance::Ptr instance);
		static Instance::Ptr getTopAppearanceInstance(Instance::Ptr instance);
	};
}
