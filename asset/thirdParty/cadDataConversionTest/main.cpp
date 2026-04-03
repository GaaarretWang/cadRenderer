#pragma once

#include <communication/dataInterface.h>
#include <string>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <Windows.h>
#else
#endif

using namespace cadDataManager;

const bool LoadByJson = true;

enum TestMode {
	allInterfaceTest,
	panelAnimation,
	wheelAnimation
};

const int currentTestMode = TestMode::allInterfaceTest;

int main(int argc, char* argv[])
{
	//---------------------------------------初始化：包括解析init.json等一些初始化操作，其他接口调用之前执行------------------------------------
	DataInterface::init();

	//---------------------------------------打包到外网测试：通过读取init.json文件来加载模型数据------------------------------------
	if (LoadByJson) {
		bool isReadLocalFBData = DataInterface::isReadLocalFBData();
		if (isReadLocalFBData) {
			//path为本地fb格式文件路径，通过init.json读取本地路径下的flatbuffer文件
			DataInterface::parseLocalModel();
		}
		else {
			bool isConvertModelByFile = DataInterface::isConvertModelByFile();
			if (isConvertModelByFile) {
				//path为本地CAD格式文件路径，直接上传本地CAD模型进行转换
				DataInterface::convertModelByFile();
			}
			else {
				//path为云端CAD格式文件路径，直接转换云端CAD模型
				DataInterface::convertModelByPath();
			}
		}
		std::unordered_map<std::string, std::vector<RenderInfo>> renderInfo = DataInterface::getRenderInfoMap();//获取显示数据Map
	}

	if (currentTestMode == TestMode::allInterfaceTest && false) {
		//1.通过上传 本地CAD模型/CAD模型在服务器的路径 到云端服务器，执行模型转换并获取模型轻量化和结构信息数据
		//std::string cadFileName = "卡通吉普车.stp";
		//std::string cadFilePath = "F:/model";
		//DataInterface::convertModelByFile("127.0.0.1", 9000, cadFileName, cadFilePath, ConversionPrecision::low);
		//DataInterface::convertModelByPath("127.0.0.1", 9000, cadFileName, cadFilePath, ConversionPrecision::low);
		//std::vector<RenderInfo> renderInfo = DataInterface::getRenderInfo();//执行完convertModel便可获取数据



		//2.通过读取本地FlatBuffer，转换、解析多个模型数据，设置模型活跃状态
		std::string fbFilePath = "../../assets/FBData/plane";
		std::string fbFileName1 = "twoSeatPartV5.fb";
		std::string fbFileName2 = "window.fb";
		std::string fbFileName3 = "twoLandingGear.fb";
		std::string fbFileName4 = "twoAirplaneBody.fb";
		DataInterface::parseLocalModel(fbFileName1, fbFilePath);
		std::vector<RenderInfo> renderInfo1 = DataInterface::getRenderInfo();//获取显示数据数组
		DataInterface::parseLocalModel(fbFileName2, fbFilePath);
		std::unordered_map<std::string, std::vector<RenderInfo>> renderInfo2 = DataInterface::getRenderInfoMap();//获取显示数据Map
		DataInterface::parseLocalModel(fbFileName3, fbFilePath);
		std::vector<RenderInfo> renderInfo3 = DataInterface::getRenderInfo();
		DataInterface::parseLocalModel(fbFileName4, fbFilePath);
		std::unordered_map<std::string, std::vector<RenderInfo>> renderInfo4 = DataInterface::getRenderInfoMap();//获取显示数据Map
		//DataInterface::setActiveDocumentData("LandingGear"); //转换了多个模型，可以通过模型名称切换活跃状态



		//3.动画、材质数据 解析/JSON获取接口 （针对驾驶舱面板模型）
		DataInterface::loadAnimationStateData("../../assets/JsonData/CockpitAnimationState.json");
		DataInterface::loadAnimationActionData("../../assets/JsonData/CockpitAnimationAction.json");
		DataInterface::loadMaterialData("../../assets/JsonData/CockpitMaterial.json");
		std::string animationStateJsonStr = DataInterface::getAnimationStateJsonStr();
		std::string animationActionJsonStr = DataInterface::getAnimationActionJsonStr();
		std::string materialJsonStr = DataInterface::getMaterialJsonStr();



		//4.模型数据获取接口：渲染几何数据、PMI数据、零件实例信息、原始FlatBuffer数据
		std::vector<RenderInfo> renderInfos = DataInterface::getRenderInfo();
		std::unordered_map<std::string, std::vector<RenderInfo>> renderInfoMap = DataInterface::getRenderInfoMap(); //key:ProtoId
		std::vector<pmiInfo> pmi = DataInterface::getPmiInfos();
		std::unordered_map<std::string, Instance::Ptr> instances = DataInterface::getInstances();
		std::unordered_map<std::string, InstanceInfo::Ptr> instanceInfos = DataInterface::getInstanceInfos();
		std::string fbModelData = DataInterface::getModelFlatbuffersData();



		//5.模型数据修改接口：可用于模型位置变换、高亮、取消高亮
		std::vector<float> matrix {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 2, 3, 0};
		std::string color {"#CC0000"};
		std::vector<int> elementIds {444};
		std::string instanceId {"172"};
		std::unordered_map<std::string, std::vector<RenderInfo>> modifyModel1 = DataInterface::modifyInstanceMatrix(instanceId, matrix);//零件实例位置的修改
		std::unordered_map<std::string, std::vector<RenderInfo>> modifyModel2 = DataInterface::modifyElementColor(instanceId, elementIds, color);//面元素外观的修改
		std::unordered_map<std::string, std::vector<RenderInfo>> moidfyModel3 = DataInterface::modifyInstanceColor(instanceId, color);//零件实例外观的修改
		std::unordered_map<std::string, std::vector<RenderInfo>> modifyModel4 = DataInterface::restoreInstanceMatrix(instanceId); //恢复零件实例到初始位姿
		std::unordered_map<std::string, std::vector<RenderInfo>> modifyModel5 = DataInterface::restoreInstanceColor(instanceId); //恢复零件实例到初始外观
		std::unordered_map<std::string, std::vector<RenderInfo>> modifyModel6 = DataInterface::restoreElementColor(instanceId, elementIds); //恢复面元素到初始外观



		//6.模型拾取接口
		std::vector<float> origin {1284.5458578, 2116.494944236, 1530.655419028};
		std::vector<float> direction{-0.4336815369, -0.72929448607, -0.52919739}; //需要归一化
		Intersection::Ptr intersection = DataInterface::pickInstance(origin, direction);


		//7.模型数据删除接口
		//DataInterface::removeModelData("大舱壁-ASM.fb");
	}


	if (currentTestMode == TestMode::panelAnimation || true) {
		//驾驶舱面板的动画流程

		//1.解析驾驶舱面板模型
		std::string fbFilePath = "../../assets/FBData";
		std::string fbFileName1 = "TUILIGAN.fb";
		std::string fbFileName2 = "ranyoukongzhi.fb";
		std::string fbFileName3 = "YIBIAOPAN.fb";
		DataInterface::parseLocalModel(fbFileName1, fbFilePath);
		DataInterface::parseLocalModel(fbFileName2, fbFilePath);
		DataInterface::parseLocalModel(fbFileName3, fbFilePath);
		//TODO 一些材质没找到对应模型 ： 因为以ProtoID为Key值，而ProtoID为UUID，每次转换都不一样。
		//DataInterface::loadMaterialData("../../../cadDataManager/assets/JsonData/CockpitMaterial.json");


		//2.加载驾驶舱面板模型的动画数据：Action + State
		DataInterface::loadAnimationStateData("../../assets/JsonData/CockpitAnimationState.json");
		DataInterface::loadAnimationActionData("../../assets/JsonData/CockpitAnimationAction.json");


		//3.获取驾驶舱面板模型的渲染显示数据 => 渲染衔接绘制模型
		std::unordered_map<std::string, std::vector<RenderInfo>> renderInfoMap = DataInterface::getRenderInfoMap(); //key:ProtoId


		//4.基于Action信息，从State查找模型位姿动画及外观状态
		std::vector<AnimationActionUnit::Ptr> animationActions = DataInterface::getAnimationActions("EngineFireAlarm");
		for (const auto& animationAction : animationActions) {
			std::string modelName = animationAction->modelName;
			std::string instanceId = animationAction->instanceId;
			std::string originState = animationAction->originState;
			std::string targetState = animationAction->targetState;
			//加载了多个模型时，需要对ModelName模型执行动画，切换该ModelName为当前活跃状态
			DataInterface::setActiveDocumentData(modelName);

			AnimationStateUnit::Ptr animationState = DataInterface::getAnimationState(modelName, instanceId);
			std::vector<AnKeyframe> anKeyframes = animationState->keyframes;
			std::vector<float> positionArray;
			std::vector<float> quaternionArray;
			for (auto& anKeyframe : anKeyframes) {
				if (anKeyframe.originState == originState && anKeyframe.targetState == targetState) {
					//TODO 找不到position因为动画Json数据对应关系没做好
					positionArray = anKeyframe.positionArray;
					quaternionArray = anKeyframe.quaternionArray;
				}
			}

			//5.调用位姿变换接口，实时更新模型位置
			if (positionArray.size() == 0) {
				spdlog::error("未找到positionArray");
				continue;
			}
			for (int i = 0; i < 11; i++) { //遍历11帧的动画数据
				std::vector<float> position = { positionArray[i * 3], positionArray[i * 3 + 1], positionArray[i * 3 + 2] };
				std::vector<float> quaternion = { quaternionArray[i * 4] , quaternionArray[i * 4 + 1], quaternionArray[i * 4 + 2],quaternionArray[i * 4 + 3] };
				std::vector<float> matrix = DataInterface::composeMatrix(position, quaternion);
				std::unordered_map<std::string, std::vector<RenderInfo>> modifyModel = DataInterface::modifyInstanceMatrix(instanceId, matrix);//零件实例位置的修改
			}

			//6.调用高亮接口，更新模型颜色
			std::vector<AnAttribute> anAttributes = animationState->attributes;
			std::vector<int> highlightElementIds;
			for (auto& anAttribute : anAttributes) {
				if (anAttribute.state == targetState) {
					highlightElementIds = anAttribute.highlightElementId;
				}
			}
			auto modifyModel = DataInterface::modifyElementColor(instanceId, highlightElementIds, "#CC0000");
		}
	}


	if (currentTestMode == TestMode::wheelAnimation || true) {
		//轮胎安装动画流程

		//1.解析轮胎面板模型
		std::string fbFilePath = "../../assets/FBData";
		std::string fbFileName = "LandingGear.fb";
		DataInterface::parseLocalModel(fbFileName, fbFilePath);


		//2.加载轮胎面板模型的动画数据：Action + State
		DataInterface::loadAnimationStateData("../../assets/JsonData/TireAnimationState.json");
		DataInterface::loadAnimationActionData("../../assets/JsonData/TireAnimationAction.json");


		//3.基于Action信息，从State查找动画对应的模型及位姿数据
		std::vector<AnimationActionUnit::Ptr> animationActions = DataInterface::getAnimationActions("TireInstallation");
		for (const auto& animationAction : animationActions) {
			std::string modelName = animationAction->modelName;
			std::string instanceId = animationAction->instanceId;
			std::string originState = animationAction->originState;
			std::string targetState = animationAction->targetState;

			std::vector<RenderInfo> instanceRenderInfo = DataInterface::getRenderInfoByInstanceId(instanceId); //根据ID获取instance的渲染数据

			AnimationStateUnit::Ptr animationState = DataInterface::getAnimationState(modelName, instanceId);
			std::vector<AnKeyframe> anKeyframes = animationState->keyframes;
			std::vector<float> positionArray;
			std::vector<float> quaternionArray;
			for (auto& anKeyframe : anKeyframes) {
				if (anKeyframe.originState == originState && anKeyframe.targetState == targetState) {
					positionArray = anKeyframe.positionArray;
					quaternionArray = anKeyframe.quaternionArray;
				}
			}

			//4.调用位姿变换接口，实时更新模型位置，并获取对应的模型
			for (int i = 0; i < 11; i++) { //遍历11帧的动画数据
				std::vector<float> position = { positionArray[i * 3], positionArray[i * 3 + 1], positionArray[i * 3 + 2] };
				std::vector<float> quaternion = { quaternionArray[i * 4] , quaternionArray[i * 4 + 1], quaternionArray[i * 4 + 2],quaternionArray[i * 4 + 3] };
				std::vector<float> matrix = DataInterface::composeMatrix(position, quaternion);
				std::unordered_map<std::string, std::vector<RenderInfo>> modifyModel = DataInterface::modifyInstanceMatrix(instanceId, matrix);//零件实例位置的修改
				//更新完之后，重新获取渲染数据，实际就是之前获取过的渲染数据的引用 （只更新矩阵不会重新构建RenderInfo，只有矩阵参数变化）
				std::vector<RenderInfo> modifyInsRenderInfo = DataInterface::getRenderInfoByInstanceId(instanceId); //根据ID获取instance的渲染数据
			}


			//5.切换到执行下一个动作动画前，将当前执行动画的模型恢复至初始位姿，并从渲染中删除
			DataInterface::restoreInstanceMatrix(instanceId);
		}

	}

#ifdef _WIN32
	Sleep(10000000);
#else
#endif

	return 0;


}