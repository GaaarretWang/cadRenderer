#pragma once

#include "global/baseDef.h"

#include "manager/assemblyManager.h"
#include "manager/elementManager.h"
#include "manager/entityManager.h"
#include "manager/protoManager.h"
#include "manager/dimensionManager.h"
#include "manager/renderUnitManager.h"
#include "manager/animationStateManager.h"

namespace cadDataManager {
	class Document3dData {
	public:
		Document3dData();
		~Document3dData();
		Document3dData(std::string docName);

		using Ptr = std::shared_ptr<Document3dData>;
		static Ptr create(std::string docName) {
			return std::make_shared<Document3dData>(docName);
		}

		std::string mDocName {""};
		std::string mModelBuffer{};
		std::string mAnimationStateBuffer{};
		AssemblyManager::Ptr mAssemblyManager{ nullptr };
		ProtoManager::Ptr mProtoManager{ nullptr };
		RenderUnitManager::Ptr mRenderUnitManager{ nullptr };
		ElementManager::Ptr mElementManager{ nullptr };
		EntityManager::Ptr mEntityManager{ nullptr };
		DimensionManager::Ptr mDimensionManager{ nullptr };
		AnimationStateManager::Ptr mAnimationStateManager{ nullptr };
	};
}