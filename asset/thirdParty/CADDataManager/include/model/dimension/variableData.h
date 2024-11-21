#pragma once
#include "flatbuffers/flatbuffers.h"
#include "parser/serialization_generated.h"

class variableData 
{
public:
	int                                            CreateType;
	const flatbuffers::String*                     NewName;
	double                                            SystemValue;
	const flatbuffers::String*                     VariableDescription;
	const flatbuffers::String*                     VariableExpression;
	const flatbuffers::String*                     VariableName;
	const flatbuffers::String*                     VariableType;
	const flatbuffers::String*                     VariableUnit;
	double                                         VariableValue;
};