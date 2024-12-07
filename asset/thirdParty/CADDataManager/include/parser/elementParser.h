#pragma once
#include "serialization_generated.h"
#include "../model/element/element.h"
#include "../global/baseDef.h"
#include "../model/element/face.h"
#include "parser/geometryParser.h"
#include "util/geometryMergeUtil.h"

class ElementParser
{
public:
	static Element::Ptr parseElement(const FlatBufferDocSpace::ElementData* elementObj);
	static Element::Ptr parseVertex(const FlatBufferDocSpace::ElementData* elementObj);
	static Element::Ptr parseEdge(const FlatBufferDocSpace::ElementData* elementObj);
	static Element::Ptr parseMesh(const FlatBufferDocSpace::ElementData* elementObj);
	static Element::Ptr parseFace(const FlatBufferDocSpace::ElementData* elementObj);
	
};