#pragma once

#include "global/baseDef.h"
#include "math/Box3.h"
#include "model/geometry/geometry.h"
#include "global/typeDefine.h"
#include "model/appearance/appearanceParams.h"
#include <spdlog/spdlog.h>

namespace cadDataManager {
	enum ConversionPrecision {
		middle,
		low,
		high
	};


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

		Box3 mGeometryBox; //���ε�OOBBʽ��Χ��
		std::vector<float> mGeometryBoxCenter; //���εİ�Χ������

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
		Box3 mGeometryBox; //���ε�OOBBʽ��Χ��

		//TODO �ؼ�������Ϣ��
		/*
			1.��ͨƽ�棺���򡣿ɼ���
			2.Բ��ƽ�棺���򡢰뾶��Բ�ġ�����
			3.���棺���򡢰뾶���϶���Բ�ġ��¶���Բ�ġ�����
			4.���棺���ġ��뾶������
			5.��ͨ���棺�ޡ�

			1.ֱ�ߣ���ʼ�㡢��ֹ�㡢���ȡ�
			2.Բ��Բ���ߣ����򣨿ɼ��㣩��Բ�ġ��뾶��

			�������ʱû����Ч��Ϣ:ֻ�й���Element�����Instance��ID��������֮�����Ͽ���ͨ��Element��������Ϣȡ����

			��������Ϣ�����ں�
		*/

	public:
		using Ptr = std::shared_ptr<ElementInfo>;
		static Ptr create() {
			return std::make_shared<ElementInfo>();
		}

		ElementInfo() = default;
		~ElementInfo() = default;

		void buildBoxInfo() {
			std::vector<float> positionArray = mGeometry->getPosition();
			mGeometryBox.setFromVector(positionArray);
		}
	};


	class InstanceInfo {
	public:
		std::string mInstanceId;
		std::string mProtoId;
		std::string mName;
		std::string mType; //instance���ͣ� ��� or װ��
		std::string mParentId;
		std::vector<std::string> mChildIds;
		std::vector<GeometryInfo::Ptr> mGeometryInfos;
		std::vector<float> mMatrixWorld;
		std::unordered_map<int, ElementInfo::Ptr> mElementInfoMap;
		std::vector<float> mInstanceAABBBox{};

	public:
		using Ptr = std::shared_ptr<InstanceInfo>;
		static Ptr create() {
			return std::make_shared<InstanceInfo>();
		}

		InstanceInfo() = default;
		~InstanceInfo() = default;

		void buildBox() {
			mInstanceAABBBox.clear();
			
			Box3 instanceBox;
			
			//��ȡ����ԭ�͵İ�Χ����Ϣ
			for (GeometryInfo::Ptr geometryInfo : mGeometryInfos) {
				Box3 box = geometryInfo->mGeometryBox;
				instanceBox.unionBox3(box);
			}

			//��Χ�и���Instance�ľ��󣬽���λ�˱任
			Matrix4 matrixWorld;
			matrixWorld.fromVector(mMatrixWorld);
			instanceBox.applyMatrix4(matrixWorld);

			//��ȡ��Χ�е�vector��
			Vector3 min = instanceBox.getMin();
			Vector3 max = instanceBox.getMax();

			mInstanceAABBBox.push_back(min.getX());
			mInstanceAABBBox.push_back(min.getY());
			mInstanceAABBBox.push_back(min.getZ());

			mInstanceAABBBox.push_back(max.getX());
			mInstanceAABBBox.push_back(max.getY());
			mInstanceAABBBox.push_back(max.getZ());
		}
	};


	struct RenderInfo
	{
		AppearanceParams::Ptr params; //�����
		Geometry::Ptr geo; //������Ϣ
		int matrixNum; //�������
		std::vector<float> matrix; //����
		std::string type; //��������
		std::string protoId;
		std::vector<std::string> instanceIds;
		

		void console() const {
			//TODO: ���ζ���������������Ƭ��������ɫ��͸����
			std::vector<float> position = this->geo->getPosition();
			std::vector<int> index = this->geo->getIndex();
			spdlog::debug("ģ�ͼ������ͣ�{}", this->type);

			if (this->type == "face") {
				spdlog::debug("ԭ������Ķ���������{}", position.size() / 3);
				spdlog::debug("ԭ���������Ƭ������{}", index.size() / 3);
				spdlog::debug("ģ����ɫ��{}", this->params->getColor());
			}
			spdlog::debug("����������{}", this->matrixNum);
			spdlog::debug("�������飺{}", fmt::join(this->matrix, ","));

			int flag = 0;
			for (auto it = this->matrix.begin(); it != this->matrix.end(); ++it) {
				if (++flag == 16) {
					flag = 0;
				}
			}
		}

		size_t getTriangleNumber() {
			if (this->type == "mesh" || this->type == "face") {
				std::vector<int> index = this->geo->getIndex();
				size_t number = index.size() / 3 * this->matrixNum;
				return number;
			}
			return 0;
		}
	};
}