# pragma once

#include "document/document3dData.h"
#include "global/baseDef.h"
namespace cadDataManager {
	class AnimationActionUnit {
	public:
		using Ptr = std::shared_ptr<AnimationActionUnit>;
		static Ptr create() {
			return std::make_shared<AnimationActionUnit>();
		}

		AnimationActionUnit() = default;
		~AnimationActionUnit() = default;

		void parse(Json animationActionData);

	public:
		std::string animationName {};
		std::string modelName {};
		std::string instanceId {};
		std::string instanceName {};
		std::string originState {};
		std::string targetState {};
		std::string type {};
		int order{};
		std::string description {};
	};

	class DocumentManager {
	public:
		Document3dData::Ptr mActiveDocument3dData;
		std::unordered_map<std::string, Document3dData::Ptr> mDocument3dDataMap{};
		std::unordered_map<std::string, std::string> mAnimationStateJsonData{}; //key:filePath, value:jsonDataString
		std::unordered_map<std::string, std::string> mAnimationActionJsonData{}; //key:filePath, value:jsonDataString
		std::unordered_map<std::string, std::string> mMaterialJsonData{}; //key:filePath, value:jsonDataString

		std::unordered_map<std::string, std::vector<AnimationActionUnit::Ptr>> mAnimationActions {}; //key:animationName, value:animationActions

		DocumentManager();
		~DocumentManager();
		static DocumentManager& getDocumentManager() {
			static DocumentManager documentManager;
			return documentManager;
		}

	public:

		Document3dData::Ptr buildDocument3dData(std::string docName);

		Document3dData::Ptr getDocument3dData(std::string docName);

		void removeDocumentData(std::string docName);

		void clearDocumentData();

		Document3dData::Ptr setActiveDocument3dData(Document3dData::Ptr activeDocument3dData);

		Document3dData::Ptr setActiveDocument3dData(std::string docName);

		Document3dData::Ptr getActiveDocument3dData();

		Document3dData::Ptr getDocument3dDataByName(std::string modelName);

		void addAnimationStateJsonData(std::string filePath, std::string animationStateJsonStr);

		std::string getAnimationStateJsonData(std::string filePath);

		void addAnimationActionJsonData(std::string filePath, std::string animationActionJsonStr);

		std::string getAnimationActionJsonData(std::string filePath);

		void addMaterialJsonData(std::string filePath, std::string materialJsonStr);
		
		std::string getMaterialJsonData(std::string filePath);

		void parseAnimationAction(Json animationActionJson);

	};
}