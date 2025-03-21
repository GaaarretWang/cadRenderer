#pragma once
#include "global/baseDef.h"

namespace cadDataManager {

	class RenderStateParams : public std::enable_shared_from_this<RenderStateParams> {
	public:
		using Ptr = std::shared_ptr<RenderStateParams>;
		static Ptr create() {
			return std::make_shared<RenderStateParams>();
		}
		RenderStateParams() {};
		~RenderStateParams() {};

		void setColor(std::string color) { mColor = color; }
		void setMatrix(std::vector<float> matrix) { mMatrix = matrix; }
		void setMaterialName(std::string materialName) { mMaterialName = materialName;}
		void setMetalness(float metalness) { mMetalness = metalness; }
		void setRoughness(float roughness) { mRoughness = roughness; }

		std::string getColor() { return mColor; }
		std::vector<float> getMatrix() { return mMatrix; }
		std::string getMaterialName() { return mMaterialName; }
		float getMetalness() { return mMetalness; }
		float getRoughness() { return mRoughness; }

	public:
		std::string				mColor{ "" };
		std::vector<float>		mMatrix{};
		std::string				mMaterialName{ "" };
		float					mMetalness{ -1 };
		float					mRoughness{ -1 };
	};
}