#pragma once
#include "element.h"
namespace cadDataManager {
	class Mesh : public Element
	{
	public:
		using Ptr = std::shared_ptr<Mesh>;
		static Ptr create() {
			return std::make_shared<Mesh>();
		}

		Mesh() {};

	private:
		ElementType         mType{ ElementType::ELEMENT_MESH };
	};
}
