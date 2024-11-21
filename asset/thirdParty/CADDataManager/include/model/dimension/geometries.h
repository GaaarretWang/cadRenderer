#pragma once
#include "flatbuffers/flatbuffers.h"
#include "parser/serialization_generated.h"


class geometriesData
{
public:
	int                                        ArcType;
	const flatbuffers::Vector<double>*         EndPnt;
	int                                        GeomId;
	const flatbuffers::String*                  InstanceId;
	const flatbuffers::Vector<double>*         MidPnt;
	const flatbuffers::Vector<double>*         PickPnt;
	int                                        SilhouetteIndex;
	int                                        SnapType;
	const flatbuffers::Vector<double>*         StartPnt;
};