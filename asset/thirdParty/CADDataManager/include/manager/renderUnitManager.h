#pragma once
#include "global/baseDef.h"
#include "renderGroup/renderUnit.h"
#include "model/proto/proto.h"
#include "model/instance/minstance.h"

/*	管理renderUnit, 类似于CC中RenderGroupController

*/
namespace cadDataManager {
	class RenderUnitManager {

	public:
		RenderUnitManager() {};
		~RenderUnitManager() {};

		using Ptr = std::shared_ptr<RenderUnitManager>;
		static Ptr create() {
			return std::make_shared<RenderUnitManager>();
		}

		void addProto(Proto::Ptr proto);
		void removeProto(std::string protoId);
		void addInstance(Instance::Ptr instance);
		void removeInstance(std::string instanceId);
		void updateRenderUnits();
		void RenderUnitManager::updateRenderUnit(RenderUnit::Ptr renderUnit);
		std::unordered_map<std::string, RenderUnit::Ptr> getProtoRenderUnitMap() { return mProtoRenderUnitMap; };
		std::unordered_map<std::string, RenderUnit::Ptr> getInstanceRenderUnitMap() { return mInstanceRenderUnitMap; };
		void modifyInstance(Instance::Ptr instance, bool needRebuildObj = false);
		std::vector<Instance::Ptr> getAllChildInstance(Instance::Ptr instance);
		
		RenderUnit::Ptr getRenderUnitByInstanceId(std::string instanceId);
		RenderUnit::Ptr getRenderUnitByProtoId(std::string protoId);

	private:

		std::unordered_map<std::string, RenderUnit::Ptr>    mProtoRenderUnitMap{};
		std::unordered_map<std::string, RenderUnit::Ptr>    mInstanceRenderUnitMap{};
	};
}
