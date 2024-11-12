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

	//TODO �ؼ�������Ϣ��
	/*
		1.��ͨƽ�棺���򡣿ɼ���
		2.Բ��ƽ�棺���򡢰뾶��Բ�ġ�����
		3.���棺���򡢰뾶���϶���Բ�ġ��¶���Բ�ġ�����
		4.���棺���ġ��뾶������
		5.��ͨ���棺�ޡ�

		1.ֱ�ߣ���ʼ�㡢��ֹ�㡢���ȡ�
		2.Բ��Բ���ߣ����򣨿ɼ��㣩��Բ�ġ��뾶��
	
		�������ʱû����Ч��Ϣ:ֻ�й���Element�����Instance��ID��������֮�����Ͽ���ͨ��Element�������Ϣȡ����

		��������Ϣ�����ں�
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
	std::string mType; //instance���ͣ� ��� or װ��
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
	AppearanceParams::Ptr params; //�����
	Geometry::Ptr geo; //������Ϣ
	int matrixNum; //�������
	std::vector<float> matrix; //����
	std::string type; //�������� face edge

	void console() const {
		//TODO: ���ζ���������������Ƭ��������ɫ��͸����
		std::vector<float> position = this->geo->getPosition();
		std::vector<int> index = this->geo->getIndex();
		std::cout << "ģ�ͼ������ͣ�" << this->type << std::endl;
		if (this->type == "face") {
			std::cout << "ԭ������Ķ���������" << position.size() / 3 << std::endl;
			std::cout << "ԭ���������Ƭ������" << index.size() / 3 << std::endl;
			std::cout << "ģ����ɫ��" << this->params->getColor() << std::endl;
		}
		//TODO: �Ƿ��ӡ�Ŀ�����
		std::cout << "����������" << this->matrixNum << std::endl;
		std::cout << "�������飺" << std::endl;
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
