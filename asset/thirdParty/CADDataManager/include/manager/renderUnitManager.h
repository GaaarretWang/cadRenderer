#pragma once
#include "global/baseDef.h"
#include "renderGroup/renderUnit.h"
#include "model/proto/proto.h"
#include "model/instance/minstance.h"

/*	管理renderUnit, 类似于CC中RenderGroupController

*/

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
	void addRenderUnit(RenderUnit::Ptr renderUnit);
	std::vector<RenderUnit::Ptr> getRenderUnitList() { return mRenderUnitList; };
	

private:

	std::unordered_map<std::string, RenderUnit::Ptr>    mProtoRenderUnitMap{};
	std::unordered_map<std::string, RenderUnit::Ptr>    mInstanceRenderUnitMap{};

	std::vector<RenderUnit::Ptr>	mRenderUnitList{};
};