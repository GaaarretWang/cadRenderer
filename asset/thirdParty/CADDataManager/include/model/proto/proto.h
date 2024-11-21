#pragma once
#include "parser/serialization_generated.h"
#include "model/element/element.h"
#include "model/element/face.h"
#include "manager/elementManager.h"
#include "manager/entityManager.h"
#include "model/entity/entity.h"
#include "manager/appearanceDataManager.h"
#include "renderGroup/renderTemplate.h"

using namespace FlatBufferDocSpace;

enum class EntityType;



class Proto : public std::enable_shared_from_this<Proto> {
public:
	using Ptr = std::shared_ptr<Proto>;
	static Ptr create() {
		return std::make_shared<Proto>();
	}

	Proto() {};
	~Proto() = default;

	void setId(std::string id) { mId = id; };
	void setDocId(std::string docId) { mDocId = docId; };
	void setVersionId(std::string versionId) { mVersionId = versionId; };
	void setName(std::string name) { mName = name; };
	void setType(std::string type) { mType = type; };
	void setEntityToList(Entity::Ptr entity) { mEntityList.push_back(entity); };
	void setRenderTemplate(RenderTemplate::Ptr r) { mRenderTemplate = r; };

	std::string getId() { return mId; };
	std::string getDocId() { return mDocId; };
	std::string getVersionId() { return mVersionId; };
	std::string getName() { return mName; };
	std::string getType() { return mType; };
	std::unordered_map<int, Element::Ptr> getElementMap() { return mElementMap; };
	std::unordered_map<int, Entity::Ptr> getEntityMap() { return mEntityMap; };
	std::vector<Entity::Ptr> getEntityList() { return mEntityList; };

	void addToElementMap(Element::Ptr element);
	void addToEntityMap(Entity::Ptr entity);
	bool existElementInMap(int id);
	bool existEntityInMap(int id);
	void removeElement(int id);
	void removeEntity(int id);
	Element::Ptr getElement(int id);
	Entity::Ptr getEntity(int id);
	RenderTemplate::Ptr getRenderTemplate() { return mRenderTemplate; };
	
	

private:
	std::string														mId{ "" };
	std::string														mDocId{ "" };
	std::string														mVersionId{ "" };
	std::string														mName{ "" };
	std::string														mType{ "" };

	std::unordered_map<int, Element::Ptr>							mElementMap{};
	std::unordered_map<int, Entity::Ptr>							mEntityMap{};
	std::vector<Entity::Ptr>										mEntityList{};
	RenderTemplate::Ptr												mRenderTemplate{nullptr};
};