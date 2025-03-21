#pragma once
#include "serialization_generated.h"
#include "datasFromDCS.h"

namespace cadDataManager {
	class ModelLoadParser {
	public:
		static void parserModelData(const uint8_t* flatbuf);

		static void loadPartDoc(const FlatBufferDocSpace::FlatBufferDoc* modelObj);

		static void loadAssemblyDoc(const FlatBufferDocSpace::FlatBufferDoc* modelObj);
	};
}
