#pragma once
#include "../parser/serialization_generated.h"
namespace cadDataManager {
	class RGeo : public std::enable_shared_from_this<RGeo> {
	public:
		using Ptr = std::shared_ptr<RGeo>;
		static Ptr create() {
			return std::make_shared<RGeo>();
		}

		RGeo() {};
		~RGeo() {};

		void setPosition(std::vector<float> position) { rPosition = position; };
		void setNormal(std::vector<float> normal) { rNormal = normal; };
		void setUV(std::vector<float> uv) { rUV = uv; };
		void setIndex(std::vector<int> index) { rIndex = index; };

		std::vector<float> getPosition() { return rPosition; };
		std::vector<float> getNormal() { return rNormal; };
		std::vector<float> getUV() { return rUV; };
		std::vector<int> getIndex() { return rIndex; };

	private:
		std::vector<float>           rPosition{};
		std::vector<float>           rNormal{};
		std::vector<float>           rUV{};
		std::vector<int>             rIndex{};
	};
}
