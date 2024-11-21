#pragma once
#include "../global/baseDef.h"
#include "../model/entity/entity.h"

class EntityManager {
private:
	std::unordered_map<int, Entity::Ptr> mEntityMap{};

public:
	EntityManager() {};
	~EntityManager() {};

	using Ptr = std::shared_ptr<EntityManager>;
	static Ptr create() {
		return std::make_shared<EntityManager>();
	}

	void addEntity(Entity::Ptr entity);

	void removeEntity(int id);

	Entity::Ptr getEntity(int id);

	bool existEntity(int id);

	void clearEntity();

	void addToEntityMap(Entity::Ptr entity);

	std::unordered_map<int, Entity::Ptr> getEntityMap() { return mEntityMap; };
};