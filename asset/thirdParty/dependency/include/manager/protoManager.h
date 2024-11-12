#pragma once
#include "../global/baseDef.h"
#include "../model/proto/proto.h"

class ProtoManager
{
private:
	std::unordered_map<std::string, Proto::Ptr> mProtoMap;

public:
	ProtoManager() {};
	~ProtoManager() {};

	using Ptr = std::shared_ptr<ProtoManager>;
	static Ptr create() {
		return std::make_shared<ProtoManager>();
	}

	void addProto(Proto::Ptr proto);

	void removeProto(std::string id);

	Proto::Ptr getProto(std::string id);

	bool existProto(std::string id);

	void clearProto();

	void addToProtoMap(Proto::Ptr proto);

	std::unordered_map<std::string, Proto::Ptr> getProtoMap() { return mProtoMap; };
};