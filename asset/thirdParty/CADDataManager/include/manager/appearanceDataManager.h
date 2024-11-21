#pragma once
#include "global/baseDef.h"
#include "parser/serialization_generated.h"


//外观数据管理类
//用于解析内核返回的外观数据AppearanceDatas
//零件文档 实体、面外观信息在 Data/AppearanceDatas下
//装配文档 实体、面外观信息在 Data/ProtoDatas/AppearanceDatas下  实例外观信息存在 Data/AppearanceDatas下


class AppearanceDataManager {
public:
	using Ptr = std::shared_ptr<AppearanceDataManager>;
	static Ptr create() {
		return std::make_shared<AppearanceDataManager>();
	}

	static AppearanceDataManager& GetInstance() {
		static AppearanceDataManager instance;
		return instance;
	}

	void addAppearanceData(const FlatBufferDocSpace::AppearanceData* appearanceObj);

	void clearAppearanceData();

	bool existAppearanceData(int id);

	void addToAppearanceMap(const FlatBufferDocSpace::AppearanceData* appearanceObj);

	std::string getAppearanceDataParamStr(int id);

	std::unordered_map<int, std::string> getAppearanceDataMap() { return appearanceDataMap; };

public:
	
	//外观数据Map
    //将内核返回的外观数据解析成Map,便于索引
    //key: appearanceDataId 外观信息索引值
    //value: appearanceDataParamStr 外观信息字符串
	
	std::unordered_map<int, std::string> appearanceDataMap;
	AppearanceDataManager() {};
	~AppearanceDataManager() {};
};