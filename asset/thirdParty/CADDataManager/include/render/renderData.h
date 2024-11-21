#pragma once
#include "../parser/serialization_generated.h"
#include "renderProto.h"
#include "renderInstance.h"
#include "renderMaterial.h"

class RenderData : public std::enable_shared_from_this<RenderData> {
public:
	using Ptr = std::shared_ptr<RenderData>;
	static Ptr create() {
		return std::make_shared<RenderData>();
	}

	static RenderData& GetInstance() {
		static RenderData instance;
		return instance;
	}

	~RenderData() {};

	void setProtoDatas(std::vector<RProto> protoDatas) { rProtoDatas = protoDatas; };
	void setBoms(std::vector<RInstance> boms) { rBoms = boms; };
	void setMaterialDatas(std::vector<RMaterial> materialDatas) { rMaterialDatas = materialDatas; };

	std::vector<RProto> getProtoDatas() { return rProtoDatas; };
	std::vector<RInstance> getBoms() { return rBoms; };
	std::vector<RMaterial> getMaterialDatas() { return rMaterialDatas; };

	void clearRenderData() { rProtoDatas.clear(); rBoms.clear(); rMaterialDatas.clear(); };
	void updateBoms(std::unordered_map<std::string, RInstance> rM) {
		std::vector<RInstance> boms;
		if (!rM.empty())
		{
			for (auto& pair : rM)
			{
				RInstance bom = pair.second;
				boms.push_back(bom);
			}
		}
		setBoms(boms);
	};


	RenderData() {};
	std::vector<RProto>              rProtoDatas;
	std::vector<RInstance>           rBoms;
	std::vector<RMaterial>           rMaterialDatas;
};