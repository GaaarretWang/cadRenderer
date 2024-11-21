#pragma once
#include "global/baseDef.h"
#include "model/dimension/modelBasedDefinition.h"
#include "math/Vector3.h"
#include "math/Line3.h"
#include "math/PlaneX.h"
#include "assemblyManager.h"

struct pmiInfo {
	std::vector<std::vector<float>> points; //(Diagonal/Horizontal: startPnt1 endPnt1 startPnt2 endPnt2) (Diameter/Radius: startPnt1远 startPnt2近 centerPnt) 世界坐标系下
	std::vector<std::vector<float>> text; // textPnt, textNormal, textDir(仅圆直径外pmi文字有)
	std::string type; // pmi type
	std::string value; // text value 
	std::string protoId; //所属proto 
	int eleId; // pmi 对应element 
	std::vector<std::vector<float>> instanceMatrixList; //对应instance matrix list
};


class DimensionManager
{
private: 
	std::unordered_map<int, ModelBasedDefinition::Ptr>			mDimensionMap{}; //用eleId区分
	std::vector<pmiInfo>										mPmiInfos{};

public:
	DimensionManager() {};
	~DimensionManager() {};

	using Ptr = std::shared_ptr<DimensionManager>;
	static Ptr create() {
		return std::make_shared<DimensionManager>();
	}

	std::unordered_map<int, ModelBasedDefinition::Ptr> getDimensionMap() { return mDimensionMap; };

	void clearDimensionMap();

	void addDimension(ModelBasedDefinition::Ptr dimension);

	void removeDimension(int id);

	ModelBasedDefinition::Ptr getDimension(int id);

	bool existDimension(int id);

	void addToDimensionMap(ModelBasedDefinition::Ptr dimension);

	void buildPmiInfo(ModelBasedDefinition::Ptr dimension, std::string protoId);

	std::vector<pmiInfo> getPmiInfoList() { return mPmiInfos; };

	void getDistancePmiInfo(ModelBasedDefinition::Ptr dimension, std::string protoId);

	void getDiameterPmiInfo(ModelBasedDefinition::Ptr dimension, std::string protoId);
};