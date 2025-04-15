#pragma once
#include "global/baseDef.h"
#include "parser/serialization_generated.h"


//外观数据管理类
//用于解析内核返回的外观数据AppearanceDatas
//零件文档 实体、面外观信息在 Data/AppearanceDatas下
//装配文档 实体、面外观信息在 Data/ProtoDatas/AppearanceDatas下  实例外观信息存在 Data/AppearanceDatas下

namespace cadDataManager {
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

		std::string getAppearanceDataParamsStr(int appearanceId, std::string docId = "");

		void addAppearanceData(const FlatBufferDocSpace::AppearanceData* appearanceObj, std::string docId = "");

		void clearAppearanceData();

	public:

		//外观数据Map
		//将内核返回的外观数据解析成Map,便于索引
		//key: appearanceDataId 外观信息索引值
		//value: appearanceDataParamStr 外观信息字符串

		/**
		 * 存储不同文档的appearanceData
		 * key: appearanceDataId (Data下的使用documentId,Data/ProtoDatas/下使用protoId)
		 * value: appearanceDataMap (该map解析并存储AppearanceDatas中的数据  key: appearanceId 外观信息索引值  value: appearanceParamStr 外观信息字符串 )
		 */
		std::unordered_map<std::string, std::unordered_map<int, std::string>> appearanceDatasMap;
		AppearanceDataManager() {};
		~AppearanceDataManager() {};
	};
}