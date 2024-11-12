#pragma once

#include "global/baseDef.h"
#include "math/Box3.h"
#include "model/geometry/geometry.h"
#include "global/typeDefine.h"
#include "model/appearance/appearanceParams.h"


class GeometryInfo {
public:
	using Ptr = std::shared_ptr<GeometryInfo>;
	static Ptr create() {
		return std::make_shared<GeometryInfo>();
	}

	GeometryInfo() = default;
	~GeometryInfo() = default;

public:
	ElementType type;
	std::vector<Geometry::Ptr> geometries;

	Box3 mGeometryBox; //几何的OOBB式包围盒
	std::vector<float> mGeometryBoxCenter; //几何的包围盒中心

	void buildBoxInfo() {
		Box3 protoGeometryBox;
		for (Geometry::Ptr geometry : geometries) {
			Box3 box;
			std::vector<float> positionArray = geometry->getPosition();
			box.setFromVector(positionArray);
			protoGeometryBox.unionBox3(box);
		}
		mGeometryBox = protoGeometryBox;
		mGeometryBoxCenter = mGeometryBox.getCenter().buildArray();
	}
};


class ElementInfo {
public:
	int mID;
	ElementType mType;
	GeomType mGeometryType;
	Geometry::Ptr mGeometry;

	//TODO 关键几何信息：
	/*
		1.普通平面：法向。可计算
		2.圆型平面：法向、半径、圆心。暂无
		3.柱面：轴向、半径、上顶面圆心、下顶面圆心。暂无
		4.球面：球心、半径。暂无
		5.普通曲面：无。

		1.直线：起始点、终止点、长度。
		2.圆与圆弧线：法向（可计算）、圆心、半径。
	
		配合中暂时没有有效信息:只有关联Element与关联Instance的ID，线与线之间的配合可以通过Element本身的信息取到。

		动画的信息放在内核
	*/

public:
	using Ptr = std::shared_ptr<ElementInfo>;
	static Ptr create() {
		return std::make_shared<ElementInfo>();
	}

	ElementInfo() = default;
	~ElementInfo() = default;
};


class InstanceInfo {
public:
	std::string mId;
	std::string mType; //instance类型： 零件 or 装配
	std::string mParentId;
	std::vector<std::string> mChildIds;
	std::vector<GeometryInfo::Ptr> mGeometryInfos;
	std::vector<float> mMatrixWorld;
	std::unordered_map<int, ElementInfo::Ptr> mElementInfoMap;

public:
	using Ptr = std::shared_ptr<InstanceInfo>;
	static Ptr create() {
		return std::make_shared<InstanceInfo>();
	}

	InstanceInfo() = default;
	~InstanceInfo() = default;
};


struct RenderInfo
{
	AppearanceParams::Ptr params; //外观类
	Geometry::Ptr geo; //几何信息
	int matrixNum; //矩阵个数
	std::vector<float> matrix; //矩阵
	std::string type; //几何类型 face edge

	void console() const {
		//TODO: 几何顶点数量、三角面片数量、颜色、透明度
		std::vector<float> position = this->geo->getPosition();
		std::vector<int> index = this->geo->getIndex();
		std::cout << "模型几何类型：" << this->type << std::endl;
		if (this->type == "face") {
			std::cout << "原型网格的顶点数量：" << position.size() / 3 << std::endl;
			std::cout << "原型网格的面片数量：" << index.size() / 3 << std::endl;
			std::cout << "模型颜色：" << this->params->getColor() << std::endl;
		}
		//TODO: 是否打印的控制器
		std::cout << "矩阵数量：" << this->matrixNum << std::endl;
		std::cout << "矩阵数组：" << std::endl;
		int flag = 0;
		for (auto it = this->matrix.begin(); it != this->matrix.end(); ++it) {
			std::cout << *it << ' ';
			if (++flag == 16) {
				std::cout << "\n";
				flag = 0;
			}
		}
	}

	size_t getTriangleNumber() {
		if (this->type == "face") {
			std::vector<int> index = this->geo->getIndex();
			size_t number = index.size() / 3 * this->matrixNum;
			return number;
		}
		return 0;
	}
};
