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

		void setPosition(std::vector<float>& addPosition);
		void setNormal(std::vector<float>& addNormal);
		void setUV(std::vector<float>& addUV);
		void setIndex(std::vector<int>& addIndex);

		std::vector<float> getPosition();
		std::vector<float> getNormal();
		std::vector<float> getUV();
		std::vector<int> getIndex();

	public:
		std::vector<float>          position;
		std::vector<float>          normal;
		std::vector<float>          uv;
		std::vector<int>            index;
	};
}
