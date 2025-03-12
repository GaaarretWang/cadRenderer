#pragma once

#include "global/baseDef.h"
#include <json.hpp>

namespace cadDataManager {
	using Json = nlohmann::json;

	class AnAttribute {
	public:
		AnAttribute() = default;
		~AnAttribute() = default;
	public:
		std::string state;
		std::vector<float> localMatrix;
		std::vector<int> highlightElementId;
	};

	class AnKeyframe {
	public:
		AnKeyframe() = default;
		~AnKeyframe() = default;
	public:
		std::string originState;
		std::string targetState;
		std::vector<float> positionArray;
		std::vector<float> quaternionArray;
	};

	class AnimationStateUnit {
	public:
		using Ptr = std::shared_ptr<AnimationStateUnit>;
		static Ptr create() {
			return std::make_shared<AnimationStateUnit>();
		}

		AnimationStateUnit() = default;
		~AnimationStateUnit() = default;

	public:
		std::string modelName;
		std::string instanceId;
		std::string instanceName;
		std::vector<std::string> allStates;
		std::string originState;
		std::vector<AnAttribute> attributes;
		std::vector<AnKeyframe> keyframes;
	};

	class AnimationStateManager {
	public:
		using Ptr = std::shared_ptr<AnimationStateManager>;
		static Ptr create() {
			return std::make_shared<AnimationStateManager>();
		}
		
		AnimationStateManager() = default;
		~AnimationStateManager() = default;

		void parse(Json animationStateData);
	public:
		std::unordered_map<std::string, AnimationStateUnit::Ptr> mAnimationStateMap{};
	};
}
