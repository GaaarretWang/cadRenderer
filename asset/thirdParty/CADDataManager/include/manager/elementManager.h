#pragma once
#include "global/baseDef.h"
#include "model/element/element.h"
namespace cadDataManager {
	class ElementManager
	{
	private:
		std::unordered_map<int, Element::Ptr> mElementMap;

	public:
		ElementManager() {};
		~ElementManager() {};

		using Ptr = std::shared_ptr<ElementManager>;
		static Ptr create() {
			return std::make_shared<ElementManager>();
		}

		void addElement(Element::Ptr element);

		void removeElement(int id);

		bool existElement(int id);

		void clearElement();

		Element::Ptr getElement(int id);

		void addToElementMap(Element::Ptr element);

		std::unordered_map<int, Element::Ptr> getElementMap() { return mElementMap; };
	};
}