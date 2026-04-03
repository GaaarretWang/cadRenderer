#pragma once
#include "global/baseDef.h"

//TODO: 定义几何数据，Element、Entity、Proto各自都构建对应（merge）层级的Geometry； 需要与外观匹配对应
namespace cadDataManager {
	class Geometry : public std::enable_shared_from_this<Geometry> {
	public:
		using Ptr = std::shared_ptr<Geometry>;
		static Ptr create() {
			return std::make_shared<Geometry>();
		}

		Geometry() = default;

		~Geometry() = default;

		void setPosition(std::vector<float>& addPosition) { position = addPosition; }
		void setNormal(std::vector<float>& addNormal) { normal = addNormal; }
		void setUV(std::vector<float>& addUV) { uv = addUV; }
		void setIndex(std::vector<int>& addIndex) { index = addIndex; }

		std::vector<float> getPosition() { return position; }
		std::vector<float> getNormal() { return normal; }
		std::vector<float> getUV() { return uv; }
		std::vector<int> getIndex() { return index; }

	public:
		std::vector<float>          position;
		std::vector<float>          normal;
		std::vector<float>          uv;
		std::vector<int>            index;
	};
}
