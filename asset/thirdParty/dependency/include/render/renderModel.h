#pragma once
#include "../parser/serialization_generated.h"
#include "renderGeometry.h"

enum  Model {
	POINTT = 0,
    LINE = 1,
    MESH = 2,
};

class RModel : public std::enable_shared_from_this<RModel> {
public:
	using Ptr = std::shared_ptr<RModel>;
	static Ptr create() {
		return std::make_shared<RModel>();
	}

	RModel() {};
	~RModel() {};

	void setType(std::string type) { rType = type; };
	void setMaterialID(int id) { rMaterialID = id; };
	void setGeo(RGeo geo) { rGeo = geo; };

	std::string getType() { return rType; };
	int getID() { return rMaterialID; };
	RGeo getGro() { return rGeo; };

private:
	std::string                 rType;
	int                         rMaterialID;
	RGeo                        rGeo;
};