#include "flatbuffers/flatbuffers.h"
#include "serialization_generated.h"
#include "datasFromDCS.h"
#include "global/typeDefine.h"
namespace cadDataManager {
	class DataParser
	{
	public:
		//Data: AppearanceDatas
		const flatbuffers::Vector<flatbuffers::Offset<FlatBufferDocSpace::AppearanceData>>* getAppearanceDatas(const FlatBufferDocSpace::FlatBufferDoc* doc);
		//通用 AppearanceDatas Id Param Size
		int32_t getAppearanceDataId(const FlatBufferDocSpace::AppearanceData* appearanceData);
		std::string getAppearanceParamStr(const FlatBufferDocSpace::AppearanceData* appearanceData);
		int getAppearanceDatasSize(const flatbuffers::Vector<flatbuffers::Offset<FlatBufferDocSpace::AppearanceData>>* appearanceDatas);

		//Data: ProtoDatas Id DocId Name Type protoAppearanceDatas
		const flatbuffers::Vector<flatbuffers::Offset<FlatBufferDocSpace::ProtoDataMessage>>* getProtoDatas(const FlatBufferDocSpace::FlatBufferDoc* doc);
		int getProtoDatasSize(const flatbuffers::Vector<flatbuffers::Offset<FlatBufferDocSpace::ProtoDataMessage>>* protoDatas);
		std::string getProtoId(const FlatBufferDocSpace::ProtoDataMessage* protoData);
		std::string getProtoDocId(const FlatBufferDocSpace::ProtoDataMessage* protoData);
		std::string getProtoName(const FlatBufferDocSpace::ProtoDataMessage* protoData);
		std::string getProtoType(const FlatBufferDocSpace::ProtoDataMessage* protoData);
		const flatbuffers::Vector<flatbuffers::Offset<FlatBufferDocSpace::AppearanceData>>* getProtoAppearanceDatas(const FlatBufferDocSpace::ProtoDataMessage* protoData);
		const flatbuffers::Vector<flatbuffers::Offset<FlatBufferDocSpace::ElementData>>* getProtoElements(const FlatBufferDocSpace::ProtoDataMessage* protoData);
		const flatbuffers::Vector<flatbuffers::Offset<FlatBufferDocSpace::EntityData>>* getProtoEntities(const FlatBufferDocSpace::ProtoDataMessage* protoData);

		//Data: Elements
		//通用 Elements Size ID
		int getElementsSize(const flatbuffers::Vector<flatbuffers::Offset<FlatBufferDocSpace::ElementData>>* elements);
		int32_t getElementID(const FlatBufferDocSpace::ElementData* element);
		int32_t getElementT(const FlatBufferDocSpace::ElementData* element);
		int32_t getElementG(const FlatBufferDocSpace::ElementData* element);
		bool getElementVisible(const FlatBufferDocSpace::ElementData* element);
		const FlatBufferDocSpace::AttributeData* getElementA(const FlatBufferDocSpace::ElementData* element);
		const flatbuffers::Vector<flatbuffers::Offset<FlatBufferDocSpace::GeometrieData>>* getElementGeometries(const FlatBufferDocSpace::ElementData* element);
		const flatbuffers::Vector<flatbuffers::Offset<FlatBufferDocSpace::DiscreteInforData>>* getElementD(const FlatBufferDocSpace::ElementData* element);

		//AI 外观索引id
		std::basic_string<char, std::char_traits<char>, std::allocator<char>> getAI(const FlatBufferDocSpace::AttributeData* attr);

		//Elements D size V
		int getElementDSize(const flatbuffers::Vector<flatbuffers::Offset<FlatBufferDocSpace::DiscreteInforData>>* D);
		const flatbuffers::Vector<float>* getElementDV(const FlatBufferDocSpace::DiscreteInforData* data);

		//Entitys ID Type Visible Attr DefData
		int getEntitiesSize(const flatbuffers::Vector<flatbuffers::Offset<FlatBufferDocSpace::EntityData>>* entities);
		int32_t getEntityID(const FlatBufferDocSpace::EntityData* entity);
		int32_t getEntityType(const FlatBufferDocSpace::EntityData* entity);
		bool getEntityVisible(const FlatBufferDocSpace::EntityData* entity);
		const FlatBufferDocSpace::AttributeData* getEntityAttr(const FlatBufferDocSpace::EntityData* entity);
		const FlatBufferDocSpace::DefDataMessage* getEntityDefData(const FlatBufferDocSpace::EntityData* entity);

		//Data InstanceNode
		std::basic_string<char, std::char_traits<char>, std::allocator<char>> getInstanceId(const FlatBufferDocSpace::CompData* instance);
		std::basic_string<char, std::char_traits<char>, std::allocator<char>> getInstanceName(const FlatBufferDocSpace::CompData* instance);
		const flatbuffers::Vector<double>* getInstanceMatrix(const FlatBufferDocSpace::CompData* instance);
		bool getInstanceVisible(const FlatBufferDocSpace::CompData* instance);
	};
}