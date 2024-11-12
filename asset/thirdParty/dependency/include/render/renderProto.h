#pragma once
#include "parser/serialization_generated.h"
#include "renderModel.h"
#include "math/Vector3.h"
#include <vector>

enum  ProtoType {
	PART = 0,
	ASSEMBLY = 1,
};

class RProto : public std::enable_shared_from_this<RProto> {
public:
	using Ptr = std::shared_ptr<RProto>;
	static Ptr create() {
		return std::make_shared<RProto>();
	}

	RProto() {};
	~RProto() {};

	void setProtoID(std::string id) { rProtoID = id; };
	void setName(std::string name) { rName = name; };
	void setType(std::string type) { rType = type; };
	void setModels(std::vector<RModel> models) { rModels = models; };
	void addModel(RModel model);
	bool existModel(RModel model);
	void setAllPnts(std::vector<Vector3> pnts) { rAllPnts = pnts; };

	std::string getID() { return rProtoID; };
	std::string getName() { return rName; };
	std::string getType() { return rType; };
	std::vector<RModel> getModels() { return rModels; };
	std::vector<Vector3> getAllPnts() { return rAllPnts; };

private:
	std::string                  rProtoID;
	std::string                  rName;
	std::string                  rType;
	std::vector<RModel>          rModels;
	std::vector<Vector3>         rAllPnts;
};