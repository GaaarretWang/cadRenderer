#pragma once
#include "parser/serialization_generated.h"
#include "geometries.h"
#include "math/Matrix4.h"


namespace cadDataManager {
	class DimensionParam : public std::enable_shared_from_this<DimensionParam> {
	public:
		using Ptr = std::shared_ptr<DimensionParam>;
		static Ptr create() {
			return std::make_shared<DimensionParam>();
		}

		DimensionParam() {};
		DimensionParam(const DimensionParam& dimension) {};
		virtual ~DimensionParam() {};


		void setDimValuePrecision(int precision);
		void setIsReference(int reference);
		void setMeasureValue(std::string value);
		void setText(std::string t);
		void setCenterPnt(const flatbuffers::Vector<double>* point);
		void setDirection(const flatbuffers::Vector<double>* direction);
		void setGeomPnts(const flatbuffers::Vector<double>* points);
		void setGeometries(int type, const flatbuffers::Vector<double>* endPnt, int geomId, const flatbuffers::String* instanceId, const flatbuffers::Vector<double>* midPnt, const flatbuffers::Vector<double>* pickPnt, int silhouetteIndex, int snapType, const flatbuffers::Vector<double>* startPnt);
		void setDimensionType(std::string type);
		void setPosition(const flatbuffers::Vector<double>* position);
		void setRefPlane(const flatbuffers::Vector<double>* plane);
		void setAngle(double a);
		void setArrowPos(int pos);
		void setColor(int c);
		void setDimColor(int dimC);
		void setTextPos(int pos);
		void setDimStatus(int status);
		void setIsAllSurfaces(int isAllSurface);
		void setLeftArrowType(int l);
		void setRightArrowType(int r);
		void setLineType(int type);
		void setLeadType(int type);
		void setLineWidth(int width);
		void setTolType(int type);
		void setDatumTargetType(int type);
		void setFrameType(int type);
		void setFontColor(int color);
		void setFontGap(int fontGap);
		void setFontName(std::string name);
		void setFontSize(double size);
		void setIsBold(int bold);
		void setIsItalic(int italic);
		void setIsUnderLine(int underLine);
		void setRelViewId(int relId);
		void setTolPrecision(int tolPrecision);
		void setSeparateFlag(int separateFlag);
		void setIsManual(int manual);
		void setManualVal(std::string manualVal);
		void setRadiusOrDiameter(int radiusOrDiameter);
		void setUseDocumentFont(bool useDocumentFont);
		void setFirstArcFlag(int firstArcFlag);
		void setSecondArcFlag(int secondArcFlag);
		void setIsThreadDim(int isThreadDim);
		void setReferenceValue(std::string value);
		void setVariable(bool v);




		int getDimValuePrecision();
		int getIsReference();
		std::string getMeasureValue();
		std::string getText();
		std::vector<double> getCenterPnt();
		std::vector<double> getDirection();
		std::vector<double> getGeomPnts();
		std::string getDimensionType();
		std::vector<double> getPosition();
		Matrix4 getRefPlane();
		double getAngle();
		int getArrowPos();
		int getColor();
		int getDimColor();
		int getTextPos();
		int getDimStatus();
		int getIsAllSurfaces();
		int getLeftArrowType();
		int getRightArrowType();
		int getLineType();
		int getLeadType();
		int getLineWidth();
		int getTolType();
		int getDatumTargetType();
		int getFrameType();
		int getFontColor();
		int getFontGap();
		std::string getFontName();
		double getFontSize();
		int getIsBold();
		int getIsItalic();
		int getIsUnderLine();
		int getRelViewId();
		int getTolPrecision();
		int getSeparateFlag();
		int getIsManual();
		std::string getManualVal();
		int getRadiusOrDiameter();
		bool getUseDocumentFont();
		int getFirstArcFlag();
		int getSecondArcFlag();
		int getIsThreadDim();
		std::string getReferenceValue();
		bool getVariable();
		std::vector<geometriesData> getGeometries() { return mGeometries; }

	private:

		int                                     mDimValuePrecision;
		int                                     mIsReference;
		std::string                             mMeasureValue;
		std::string                             mText;
		std::vector<double>                     mCenterPnt;
		std::vector<double>                     mDirection;
		std::vector<double>                     mGeomPnts;  //先取这个前两个做startPnt1/2, 若有mGeometries，改成这个
		std::vector<geometriesData>             mGeometries; //和modelBasedDefinition里相同 没写get
		std::string                             mDimensionType;
		std::vector<double>                     mPosition; //TextPosition
		Matrix4									mRefPlane;
		double                                  mAngle;
		int                                     mArrowPos;
		int                                     mColor;
		int                                     mDimColor;
		int                                     mTextPos;
		int                                     mDimStatus;
		int                                     mIsAllSurfaces;
		int                                     mLeftArrowType;
		int                                     mRightArrowType;
		int                                     mLineType;
		int                                     mLeadType;
		int                                     mLineWidth;
		int                                     mTolType;
		int                                     mDatumTargetType;
		int                                     mFrameType;
		int                                     mFontColor;
		int                                     mFontGap;
		std::string                             mFontName;
		double                                  mFontSize;
		int                                     mIsBold;
		int                                     mIsItalic;
		int                                     mIsUnderLine;
		int                                     mRelViewId;
		int                                     mTolPrecision;
		int                                     mSeparateFlag;
		int                                     mIsManual;
		std::string                             mManualVal;
		int                                     mRadiusOrDiameter;
		bool                                    mUseDocumentFont;
		int                                     mFirstArcFlag; //圆弧条件1
		int                                     mSecondArcFlag; //圆弧条件2
		int                                     mIsThreadDim;
		std::string                             mReferenceValue; //text的文字值
		bool                                    mVariable;
	};
}
