#pragma once
#include "serialization_generated.h"
#include "../model/proto/proto.h"

struct renderTemplatePre {
	std::vector<Entity::Ptr>     rEntityList;
	std::vector<Element::Ptr>  rElementList;
};

class ProtoParser
{
public:
	void parseProto(const FlatBufferDocSpace::ProtoDataMessage* protoObj);
	void buildRenderTemplate(Proto::Ptr proto);
	renderTemplatePre buildRenderTemplatePre(Proto::Ptr proto);
	void computeBoundingBox(Proto::Ptr proto);
};