#pragma once
#include "element.h"
namespace cadDataManager {
	class Vertex : public Element
	{
	public:
		using Ptr = std::shared_ptr<Vertex>;
		static Ptr create() {
			return std::make_shared<Vertex>();
		}

		Vertex() {};

	private:
		ElementType         mType{ ElementType::ELEMENT_VERTEX };
	};
}