#pragma once
#include "element.h"
#include "model/appearance/appearanceParams.h"
namespace cadDataManager {
	class Face : public Element {
	public:
		using Ptr = std::shared_ptr<Face>;
		static Ptr create() {
			return std::make_shared<Face>(); 
		}

		Face() = default;
		~Face() = default;


	private:
		ElementType         mType{ ElementType::ELEMENT_FACE };

	};
}
