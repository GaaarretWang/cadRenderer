#pragma once
#include "parser/serialization_generated.h"
#include "math/Box3.h"
#include "math/Matrix4.h"

class RInstance : public std::enable_shared_from_this<RInstance> {
public:
	using Ptr = std::shared_ptr<RInstance>;
	static Ptr create() {
		return std::make_shared<RInstance>();
	}

	RInstance() {};
	~RInstance() {};

	void setID(std::string id) { rInstanceID = id; };
	void setMaterialID(int mID) { rMaterialID = mID; };
	void setProtoID(std::string protoID) { rProtoID = protoID; };
	void setParentID(std::string parentID) { rParentID = parentID; };
	void setMatrix(std::vector<double> m) { rMatrix = m; };
	void setName(std::string name) { rName = name; };
	void setBox3();
	void setBox(Box3 box) { rBox3 = box; };

	std::string getID() { return rInstanceID; };
	int getMaterialID() { return rMaterialID; };
	std::string getProtoID() { return rProtoID; };
	std::string getParentID() { return rParentID; };
	std::vector<double> getM() { return rMatrix; };
	std::string getName() { return rName; };
	Box3 getBox() { return rBox3; };
	std::vector<float> getBoxMin();
	std::vector<float> getBoxMax();
	bool isBox3Empty();
	std::vector<float> getBoxVector();

private:
	std::string                   rInstanceID;
	int                           rMaterialID;
	std::string                   rProtoID;
	std::string                   rParentID;
	std::vector<double>           rMatrix;
	std::string                   rName;
	Box3                          rBox3;
};