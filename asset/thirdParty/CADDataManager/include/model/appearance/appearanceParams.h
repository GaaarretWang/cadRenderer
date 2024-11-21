#pragma once
#include "global/baseDef.h"

class AppearanceParams : public std::enable_shared_from_this<AppearanceParams> {
public:
	using Ptr = std::shared_ptr<AppearanceParams>;
	static Ptr create() {
		return std::make_shared<AppearanceParams>();
	}
	AppearanceParams() {};
	~AppearanceParams() {};

	void parse(std::string appearanceParamsStr);
	std::string getParamsStr();
	void clear();
	AppearanceParams::Ptr clone(AppearanceParams::Ptr apperanceParams);
	bool isDefault();
	bool isNotDefault();

	void setColor(std::string color) { mColor = color; };
	void setOpacity(float opacity) { mOpacity = opacity; };

	std::string getColor() { return mColor; };
	float getOpacity() { return mOpacity; };


public:
	std::string                  mColor{ "#BBBBBB" };
	float                        mOpacity{ 1 };
	std::string                  mEmissive{ "#FFFFFF" };
	float                        mEmissiveIntensity{ 1 };
	std::string                  mSpecular{ "#666666" };
	float                        mSpecularIntensity{ 1 };
	float                        mShininess{ 10 };
	float                        mMetalness{ 0 };
	float                        mRoughness{ 0.2 };
	float                        mTransmission{ 0 };
};