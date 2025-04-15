#pragma once
#include "serialization_generated.h"
#include "../model/proto/proto.h"
namespace cadDataManager {

	class ProtoParser
	{
	public:
		void parseProto(const FlatBufferDocSpace::ProtoDataMessage* protoObj);
		void buildRenderTemplate(Proto::Ptr proto);
		void computeBoundingBox(Proto::Ptr proto);
	};
}