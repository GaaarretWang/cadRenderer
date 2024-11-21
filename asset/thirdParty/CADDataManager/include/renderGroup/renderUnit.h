#pragma once
#include "global/baseDef.h"
#include "model/geometry/geometry.h"
#include "renderGroup/renderTemplate.h"
#include "model/instance/minstance.h"

// 渲染类型  

struct RenderTypes
{
	bool normal; // 实例未被设置过外观
	bool appearnace; // 实例被设置过外观
};

struct BaseInfo
{
	std::string          appearanceParamStr;
	Geometry::Ptr        geometry;
	int                  matrixNum;
	std::vector<float>   matrix;
	std::string          type;
};

class RenderUnit : public std::enable_shared_from_this<RenderUnit> {
public:
	using Ptr = std::shared_ptr<RenderUnit>;
	static Ptr create() {
		return std::make_shared<RenderUnit>();
	}

	RenderUnit()= default;
	~RenderUnit() = default;
	
	void dispose();
	bool addInstance(Instance::Ptr instance);
	void removeInstance(std::string instanceId);
	void buildObj3d();
	void updateInstanceMatrix();
	void setRenderTemplate(RenderTemplate::Ptr rm) { mRenderTemplate = rm; };
	std::vector<BaseInfo> getRenderUnit() { return mRenderUnit; };

private:
	RenderTemplate::Ptr                             mRenderTemplate;
	std::unordered_map<std::string, GeometryInfo::Ptr>	mAppStrLines{}; // key:appearanceParamsStr value:geometry, edge/curve
	std::unordered_map<std::string, GeometryInfo::Ptr>	mAppStrMeshs{}; // key:appearanceParamsStr value:geometry, face/entity
	std::unordered_map<std::string, Geometry::Ptr>  mAppStrInsMeshs{}; // key:appearanceParamsStr value:geometry, instance
	std::unordered_map<std::string, Instance::Ptr>  mInstances{};
	std::vector<BaseInfo>                           mRenderUnit{};
	RenderTypes calcRenderTypes();
};