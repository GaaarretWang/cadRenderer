#pragma once
#include "element.h"


class Edge : public Element 
{
public:
	using Ptr = std::shared_ptr<Edge>;
	static Ptr create() {
		return std::make_shared<Edge>();
	}

	Edge() {};

private:
	ElementType         mType{ ElementType::ELEMENT_EDGE };
};