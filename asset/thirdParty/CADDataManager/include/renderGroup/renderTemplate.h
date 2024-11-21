#pragma once
#include "global/baseDef.h"
#include "model/appearance/appearanceParams.h"
#include "global/typeDefine.h"
#include "model/geometry/geometry.h"
#include "model/element/element.h"
#include  "global/typeDefine.h"
#include "util/geometryMergeUtil.h"
#include "communication/dataStructure.h"

class RenderTemplate : public std::enable_shared_from_this<RenderTemplate> {
public:
	using Ptr = std::shared_ptr<RenderTemplate>;
	static Ptr create() {
		return std::make_shared<RenderTemplate>();
	}

	RenderTemplate() = default;
	~RenderTemplate() = default;


	void buildGeometry(std::vector<Element::Ptr> elementList);
	void groupElementsByAppearanceParams(Element::Ptr element);
	void merge();

	std::unordered_map<std::string, GeometryInfo::Ptr> getMaterialMergeInfoMap() { return materialMergeInfoMap; };

private:
	std::unordered_map<std::string, GeometryInfo::Ptr> materialMergeInfoMap{};   // key: appearanceParams    value: elementInfo
	std::vector<Element::Ptr> meshElementList{};   //暂时没写 element基类未加entityRef

};
