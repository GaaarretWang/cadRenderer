#pragma once
#include "parser/serialization_generated.h"
#include "global/typeDefine.h"
#include "arrayD.h"
#include "model/geometry/geometry.h"
#include "model/appearance/appearanceParams.h"
#include "model/attribute/renderStateParams.h"

namespace cadDataManager {
	class Element : public std::enable_shared_from_this<Element> {
	public:
		using Ptr = std::shared_ptr<Element>;
		static Ptr create() {
			return std::make_shared<Element>();
		}

		Element() {};
		Element(const Element& element) {};
		virtual ~Element() {};

		virtual void fakeFunction() {}

		int getId() { return mId; };
		ElementType getType() { return mType; };
		bool getVisible() { return mVisible; };
		GeomType getG() { return mG; };


		void setId(int id) { mId = id; };
		void setType(ElementType type) { mType = type; };
		void setVisible(bool visible) { mVisible = visible; };
		void setG(GeomType G) { mG = G; };

		//mD
		void setD(int  FT, int FN, const flatbuffers::Vector<int>* I, const flatbuffers::Vector<unsigned short>* IC, const flatbuffers::Vector<float>* N, const flatbuffers::Vector<unsigned short>* NC, const flatbuffers::Vector<float>* V, const flatbuffers::Vector<float>* TE, int VN, int VO);

		void setFT(int index, int FT);
		void setFN(int index, int FN);
		void setI(int index, const flatbuffers::Vector<int>* I);
		void setIC(int index, const flatbuffers::Vector<unsigned short>* IC);
		void setN(int index, const flatbuffers::Vector<float>* N);
		void setNC(int index, const flatbuffers::Vector<unsigned short>* NC);
		void setV(int index, const flatbuffers::Vector<float>* V);
		void setVN(int index, int VN);
		void setVO(int index, int VO);
		void setGeometry(Geometry::Ptr g) { mGeometry = g; };
		void setAppearanceParam(AppearanceParams::Ptr appearance) { mAppearanceParam = appearance; };
		void setRenderStateParam(RenderStateParams::Ptr renderStateParam) { mRenderStateParam = renderStateParam; };
		void setEntityId(int entityId) { mEntityId = entityId; };

		std::vector<arrayD> getD() { return mD; };
		int getDSize() { return mD.size(); };

		int getFT(int index) { return mD[index].FT; };
		int getFN(int index) { return mD[index].FN; };
		const flatbuffers::Vector<int>* getI(int index) { return mD[index].I; };
		const flatbuffers::Vector<unsigned short>* getIC(int index) { return mD[index].IC; };
		const flatbuffers::Vector<float>* getN(int index) { return mD[index].N; };
		const flatbuffers::Vector<unsigned short>* getNC(int index) { return mD[index].NC; };
		const flatbuffers::Vector<float>* getV(int index) { return mD[index].V; };
		int getVN(int index) { return mD[index].VN; };
		int getVO(int index) { return mD[index].VO; };
		const flatbuffers::Vector<float>* getTE(int index) { return mD[index].TE; };
		Geometry::Ptr getGeometry() { return mGeometry; };
		AppearanceParams::Ptr getAppearanceParam() { 
			if (mRenderStateParam != nullptr && !mRenderStateParam->getColor().empty()) {
				AppearanceParams::Ptr appearanceParams;
				if (mAppearanceParam == nullptr) {
					appearanceParams = AppearanceParams::create();
				}
				else {
					appearanceParams = mAppearanceParam->clone();
				}
				appearanceParams->setColor(mRenderStateParam->getColor());
				return appearanceParams;
			}
			else {
				return mAppearanceParam;
			}
		};
		RenderStateParams::Ptr getRenderStateParam() { return mRenderStateParam; };
		int getEntityId() { return mEntityId; };

	protected:
		int									mId{ 0 };
		ElementType							mType{ ElementType::ELEMENT_UNKNOWN };
		bool								mVisible{ true };

		GeomType							mG{ GeomType::GEOM_GEOM_UNKNOWN };
		std::vector<arrayD>					mD;
		Geometry::Ptr						mGeometry{ nullptr };
		AppearanceParams::Ptr				mAppearanceParam{ nullptr };
		RenderStateParams::Ptr				mRenderStateParam{ nullptr };
		int									mEntityId{ 0 };
	};
}