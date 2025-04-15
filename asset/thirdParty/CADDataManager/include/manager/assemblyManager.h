#pragma once
#include "model/instance/minstance.h"
#include "global/baseDef.h"
#include "communication/dataStructure.h"
namespace cadDataManager {
	class AssemblyManager : public std::enable_shared_from_this<AssemblyManager> {
	public:
		AssemblyManager() {};
		~AssemblyManager() {};

		using Ptr = std::shared_ptr<AssemblyManager>;
		static Ptr create() {
			return std::make_shared<AssemblyManager>();
		};

		//about mInstanceMap
		void addInstance(Instance::Ptr instance);
		void deleteInstance(std::string id);
		std::unordered_map<std::string, Instance::Ptr> getInstanceMap() { return mInstanceMap; };
		Instance::Ptr getInstance(std::string id); //get instance based on instanceId
		Instance::Ptr getTopInstance(std::string id);
		bool isSameTopInstance(std::string curId, std::string refId);
		void clearInstance();
		void clearInstanceTree();

		//about mInstanceTreeList
		void buildInstanceTree(const flatbuffers::Vector<flatbuffers::Offset<FlatBufferDocSpace::InstanceNode>>* insTreeObj);
		std::vector<Instance::Ptr> _buildTree(const flatbuffers::Vector<flatbuffers::Offset<FlatBufferDocSpace::InstanceNode>>* treeObj);
		std::vector<std::vector<Instance::Ptr>> getInstanceTreeList() { return mInstanceTreeList; };

		std::vector<Instance::Ptr> getInstancesByProto(std::string protoId);

		void buildInstanceInfoMap();
		void updateInstanceInfoMap();
		void updateInstanceInfo(Instance::Ptr instance);

		std::unordered_map<std::string, InstanceInfo::Ptr> getInstanceInfoMap();

	private:
		std::vector<std::vector<Instance::Ptr>>					mInstanceTreeList{};
		std::unordered_map<std::string, Instance::Ptr>			mInstanceMap{};
		std::unordered_map<std::string, InstanceInfo::Ptr>		mInstanceInfoMap{};
	};
}