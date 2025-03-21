#pragma once
#include "parser/serialization_generated.h"
#include "global/typeDefine.h"
#include "geometries.h"
#include "variableData.h"
#include "dimensionParam.h"
namespace cadDataManager {
	class ModelBasedDefinition : public std::enable_shared_from_this<ModelBasedDefinition> {
	public:
		using Ptr = std::shared_ptr<ModelBasedDefinition>;
		static Ptr create() {
			return std::make_shared<ModelBasedDefinition>();
		}

		ModelBasedDefinition() {};
		ModelBasedDefinition(const ModelBasedDefinition& modelBasedDefinition) {};
		virtual ~ModelBasedDefinition() {};

		void setId(int id);
		void setHi(int hi);
		void setType(ElementType type);
		void setViewIndex(std::string index);
		void setTopoId(std::string topoId);
		void setVisible(bool visible);
		void setDimensionType(std::string type);
		void setDimVariable(int type, const flatbuffers::String* newName, double systemValue, const flatbuffers::String* description, const flatbuffers::String* expression, const flatbuffers::String* name, const flatbuffers::String* variableType, const flatbuffers::String* unit, double value);
		void setStatus(int status);
		void setGeometries(int type, const flatbuffers::Vector<double>* endPnt, int geomId, const flatbuffers::String* instanceId, const flatbuffers::Vector<double>* midPnt, const flatbuffers::Vector<double>* pickPnt, int silhouetteIndex, int snapType, const flatbuffers::Vector<double>* startPnt);
		void setSheetFormatText(bool text);
		void setDetailLabelFlag(int flag);
		void setMatrix(const flatbuffers::Vector<double>* matrix);
		void setFeatureIndex(std::string index);
		void setParamName(std::string name);
		void setParamIndex(int index);
		void setRelViewId(int relViewId);
		void setDimensionParam(DimensionParam::Ptr param);

		int getId();
		int getHi();
		ElementType getType();
		std::string getViewIndex();
		std::string getTopoId();
		bool getVisible();
		std::string getDimensionType();
		variableData getDimVariable();
		int getCreateType();
		const flatbuffers::String* getNewName();
		double getSystemValue();
		const flatbuffers::String* getVariableDescription();
		const flatbuffers::String* getVariableExpression();
		const flatbuffers::String* getVariableName();
		const flatbuffers::String* getVariableType();
		const flatbuffers::String* getVariableUnit();
		double getVariableValue();
		int getStatus();
		geometriesData getGeometryDataArcType(int index); // 拿哪个 待补充
		int getArcType(int index);
		const flatbuffers::Vector<double>* getEndPnt(int index);
		int getGeomId(int index);
		const flatbuffers::Vector<double>* getMidPnt(int index);
		const flatbuffers::Vector<double>* getPickPnt(int index);
		int getSilhouetteIndex(int index);
		int getSnapType(int index);
		const flatbuffers::Vector<double>* getStartPnt(int index);
		bool getSheetFormatText();
		int getDetailLabelFlag();
		std::vector<double> getTransformMatrix();
		std::string getFeatureIndex();
		std::string getParamName();
		int getParamIndex();
		int getRelViewId();
		DimensionParam::Ptr getDimensionParam();

	protected:
		int                                     mId{ 0 };
		int                                     mHi{ 0 }; //TopoId
		ElementType                             mType{ ElementType::ELEMENT_UNKNOWN };
		std::string                                     mViewIndex{ 0 };
		std::string                             mTopoId{""};
		bool                                    mVisible{ true };

		std::string                             mDimensionType;
		variableData                            mDimVariable;
		int                                     mStatus{ 0 };
		std::vector<geometriesData>             mGeometries;
		bool                                    mSheetFormatText{ false };
		int                                     mDetailLabelFlag{ 0 };
		std::vector<double>                     mTransformMatrix;
		std::string                             mFeatureIndex;
		std::string                             mParamName{""};
		int                                     mParamIndex{ 0 };
		int                                     mRelViewId{ 0 };

		DimensionParam::Ptr                          mDimensionParam;
	};
}
