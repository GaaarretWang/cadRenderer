#pragma once
#include "flatbuffers/flatbuffers.h"
#include "parser/serialization_generated.h"
namespace cadDataManager {
	//TODO: D是内核的结构，应该进一步解析
	class arrayD
	{
	public:
		int                                                                                       FT;
		int                                                                                       FN;
		const flatbuffers::Vector<int>* I;
		const flatbuffers::Vector<unsigned short>* IC;
		const flatbuffers::Vector<float>* N;
		const flatbuffers::Vector<unsigned short>* NC;
		const flatbuffers::Vector<float>* V;
		const flatbuffers::Vector<float>* TE;
		int                                                                                       VN;
		int                                                                                       VO;

	};
}