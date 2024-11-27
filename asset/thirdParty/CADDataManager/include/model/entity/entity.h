#pragma once
#include "parser/serialization_generated.h"
#include "global/typeDefine.h"
#include "model/appearance/appearanceParams.h"


class Entity : public std::enable_shared_from_this<Entity> {
public:
    using Ptr = std::shared_ptr<Entity>;
    static Ptr create() {
        return std::make_shared<Entity>();
    }

    Entity() {};
    ~Entity() {};

    void setId(int id) { mId = id; };
    void setType(EntityType type) { mType = type; };
    void setName(std::string name) { mName = name; };
    void setVisible(bool visible) { mVisible = visible; };

    void setAppearanceParam(AppearanceParams::Ptr appearance) { mAppearanceParam = appearance; };
    void setElementList(int id) { mElementList.push_back(id); };
    

    int getId() { return mId; };
    EntityType getType() { return mType; };
    std::string getName() { return mName; };
    bool getVisible() { return mVisible; };
    std::vector<int> getElementList() { return mElementList; };
    
    AppearanceParams::Ptr getAppearanceParam() { return mAppearanceParam; };

    void setTopoData(const FlatBufferDocSpace::SolidSketchTopoData* topoData) { mTopoData = topoData; };
    const FlatBufferDocSpace::SolidSketchTopoData* getTopoData() { return mTopoData; };

private:
    int                                                                                          mId{ 0 };
    EntityType                                                                              mType{EntityType::ENTITY_UNKNOWN };
    std::string                                                                               mName{ "" };
    bool                                                                                       mVisible{ true };
    std::string                                                                               mAI{ "" };

    AppearanceParams::Ptr                                                                               mAppearanceParam{  };
    const FlatBufferDocSpace::SolidSketchTopoData*                  mTopoData;
    std::vector<int>                                                                     mElementList;

};
  
