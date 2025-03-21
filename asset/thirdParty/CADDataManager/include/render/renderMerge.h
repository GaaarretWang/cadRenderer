#pragma once
#include "manager/elementManager.h"
#include "parser/serialization_generated.h"
#include "renderModel.h"
#include "renderMaterial.h"
#include "renderProto.h"
#include "renderInstance.h"
#include "model/proto/proto.h"
#include "model/instance/minstance.h"
#include "renderData.h"
#include "math/Vector3.h"
#include "math/Matrix4.h"
namespace cadDataManager {
	class RMerge
	{
	public:
		void merge(std::unordered_map<std::string, Instance::Ptr> insMap, std::unordered_map<std::string, Proto::Ptr> protoMap, std::unordered_map<int, std::string> appMap);

		std::vector<RInstance> mergeBoms(std::unordered_map<std::string, Instance::Ptr> map);

		std::vector<RProto> mergeProtoDatas(std::unordered_map<std::string, Proto::Ptr> map);

		std::vector<RMaterial> mergeMaterialData(std::unordered_map<int, std::string> map);

		std::vector<RModel> mergeModel(std::unordered_map<int, Element::Ptr> elementMap);

		RModel createModel(std::unordered_map<int, Element::Ptr> map, std::string type, int id);

		RGeo::Ptr mergeGeo(std::unordered_map<int, Element::Ptr> map);

		std::vector<int> generateFaceIndex1(int arrayLength);

		std::vector<int> generateFaceIndex2(int arrayLength);

		std::vector<Vector3> mergePnts(std::vector<RModel> models);

		void setAssembleBox(RInstance r);
	};
}
