#pragma once
#include "global/baseDef.h"
#include "model/geometry/geometry.h"
#include "renderGroup/renderTemplate.h"
#include "model/instance/minstance.h"
#include "communication/dataStructure.h"


// 渲染类型  
namespace cadDataManager {
	struct RenderTypes
	{
		bool normal; // 实例未被设置过外观
		bool appearnace; // 实例被设置过外观
	};

	class RenderUnit : public std::enable_shared_from_this<RenderUnit> {
	public:
		using Ptr = std::shared_ptr<RenderUnit>;
		static Ptr create() {
			return std::make_shared<RenderUnit>();
		}

		RenderUnit() = default;
		~RenderUnit() = default;

		void dispose();
		bool addInstance(Instance::Ptr instance);
		void removeInstance(std::string instanceId);
		void buildObj3d();
		void updateRenderInfos();
		void updateInstanceMatrix();
		void setRenderTemplate(RenderTemplate::Ptr rm) { mRenderTemplate = rm; };
		std::vector<RenderInfo> getRenderInfos() { return mRenderInfos; };
		std::vector<RenderInfo> buildInstanceRenderInfos(std::string instanceId);
		std::vector<RenderInfo> getInstanceRenderInfos(std::string instanceId);
		std::vector<std::string> getInstanceIds();
		std::string getProtoId();

	public:
		RenderTemplate::Ptr									mRenderTemplate;
		std::unordered_map<std::string, GeometryInfo::Ptr>	mAppStrLines{}; // key:appearanceParamsStr value:geometry, edge/curve
		std::unordered_map<std::string, GeometryInfo::Ptr>	mAppStrMeshs{}; // key:appearanceParamsStr value:geometry, face/entity
		std::unordered_map<std::string, Geometry::Ptr>		mAppStrInsMeshs{}; // key:appearanceParamsStr value:geometry, instance
		std::unordered_map<std::string, Instance::Ptr>		mInstances{};
		std::vector<RenderInfo>								mRenderInfos{};
		std::unordered_map<std::string, std::vector<RenderInfo>> mInstanceRenderInfos{};
		Geometry::Ptr										mMergeGeometry;

		RenderTypes calcRenderTypes();
	};
}