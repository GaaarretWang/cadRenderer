/*
	* 本文档定义对外提供的通信链接、数据转换、数据获取的接口
	* cadar青岛服务器： host 172.22.10.69； port 12345
*/

#pragma once

#include "global/baseDef.h"
#include "flatbuffers/flatbuffers.h"
#include "renderGroup/renderUnit.h"
#include "manager/dimensionManager.h"
#include "communication/request.h"
#include "communication/dataStructure.h"
#include <json.hpp>
#include "math/Raycaster.h"

#ifdef _WIN32
#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#endif

using json = nlohmann::json;

namespace cadDataManager {

	static int port;

#ifdef _WIN32
	static SOCKET socketInstance;
#else
	static int socketInstance;
#endif


	class DataInterface {

	public: 
		DataInterface& GetInstance() {
			static DataInterface instance;
			return instance;
		}

		//----------------------------------------初始化------------------------------------------------------------------------------------------------------------------------------
		//初始化：读取init.json文件
		static void init();
		//从init.json中获取信息：从本地读取FB文件 or 从云端转换
		static bool isReadLocalFBData();
		//从init.json中获取信息：转换本地CAD文件 or 转换云端CAD文件
		static bool isConvertModelByFile();




		//-----------------------------------------通过本地读取FlatBuffer文件，获取转换后的模型数据：分为init.json读参、直接传参两个版本-----------------------------------------------
		//解析本地CAD（flatBuffer）数据：仅从本地读取时调用
		static void parseLocalModel();
		static void parseLocalModel(std::string fileName, std::string filePath);




		//-----------------------------------------通过HTTP请求，执行模型转换：分为init.json读参、直接传参两个版本-----------------------------------------------
		//通过HTTP从云端加载： 读取云端路径下的模型
		static void convertModelByPath();
		static void convertModelByPath(std::string host, int port, std::string fileName, std::string filepath, int precision = ConversionPrecision::middle);
		
		//通过HTTP从云端加载： 本地路径下的模型上传
		static void convertModelByFile();
		static void convertModelByFile(std::string host, int port, std::string fileName, std::string filepath, int precision = ConversionPrecision::middle);




		//-----------------------------------------建立Socket链接，执行模型转换：分为init.json读参、直接传参两个版本-----------------------------------------------
		//与云端建立Socket通信
		static void buildCloudCommunication();
		static void buildCloudCommunication(std::string host, int port);
		
		//通过Socket发送CAD模型数据转换请求
		static void convertModelData();
		static void convertModelData(std::string fileName, std::string filePath, int precision);




		//-----------------------------------------对转换的模型进行解析、管理、编辑、删除-----------------------------------------------

		//CAD模型数据解析：从本地读取的CAD数据或从云端加载的CAD数据，均通过该方法解析，该方法内部自动调用
		static void parseModelData(std::string fileName, char* modelBuffer, size_t modelBufferSize);

		//同时加载多个模型时，切换某一个模型为活跃状态
		static void setActiveDocumentData(std::string fileName);

		//同时加载多个模型时，删除其中某一个模型
		static void removeModelData(std::string fileName);

		//清空当前已解析的所有模型数据
		static void disposeDataManager();




		//-----------------------------------------数据获取接口-----------------------------------------------
		
		//渲染数据获取接口: 获取（当前活跃模型）某一个零件的几何数据
		static std::vector<RenderInfo> getRenderInfoByProtoId(std::string protoId, bool printLog = false);

		//渲染数据获取接口: 获取（当前活跃模型）所有零件几何数据的列表。 （推荐使用下面的getRenderInfoMap获取模型数据）
		static std::vector<RenderInfo> getRenderInfo(bool printLog = false);
		
		//渲染数据获取接口: 获取（当前活跃模型）所有零件几何数据的Map，key值为零件ID。 （推荐以Map形式获取，方便进行模型的更新）
		static std::unordered_map<std::string, std::vector<RenderInfo>> getRenderInfoMap(bool printLog = false);

		//pmi获取接口：获取（当前活跃模型）所有PMI数据
		static std::vector<pmiInfo> getPmiInfos(bool printLog = false);

		//instance获取接口：返回（当前活跃模型）完整的Instance数据，Instance参数结构会较为复杂
		static std::unordered_map<std::string, Instance::Ptr> getInstances(bool printLog = false);

		//instanceInfo获取接口：InstanceInfo是对instance进行简化，只提供拾取交互必要的信息，可以根据需求扩展
		static std::unordered_map<std::string, InstanceInfo::Ptr> getInstanceInfos();

		//获取转换得到的（当前活跃模型）FlatBuffer数据
		static std::string getModelFlatbuffersData();




		//-----------------------------------------模型信息修改，并返回修改后的数据-----------------------------------------------
		
		//修改（当前活跃模型）元素的外观
		static std::unordered_map<std::string, std::vector<RenderInfo>> modifyElementColor(std::string instanceId, std::vector<int> elementIds, std::string color);
		
		//恢复（当前活跃模型）元素的外观
		static std::unordered_map<std::string, std::vector<RenderInfo>> restoreElementColor(std::string instanceId, std::vector<int> elementIds);
	
		//修改（当前活跃模型）实例的外观
		static std::unordered_map<std::string, std::vector<RenderInfo>> modifyInstanceColor(std::string instanceId, std::string color);

		//恢复（当前活跃模型）实例的外观
		static std::unordered_map<std::string, std::vector<RenderInfo>> restoreInstanceColor(std::string instanceId);

		//修改（当前活跃模型）实例的位姿矩阵
		static std::unordered_map<std::string, std::vector<RenderInfo>> modifyInstanceMatrix(std::string instanceId, std::vector<float> matrix);

		//恢复（当前活跃模型）实例的位姿矩阵
		static std::unordered_map<std::string, std::vector<RenderInfo>> restoreInstanceMatrix(std::string instanceId);





		//-----------------------------------------JSON数据加载解析（目前只针对驾驶舱交互模型）-----------------------------------------------
		//加载动画状态数据 : 空参时会从init.json中读取参数
		static void loadAnimationStateData(std::string filePath = "");

		//获取动画状态原始JSON数据
		static std::string getAnimationStateJsonStr(std::string filePath = "");

		//加载动画动作说明数据
		static void loadAnimationActionData(std::string filePath = "");

		//获取动画动作原始JSON数据
		static std::string getAnimationActionJsonStr(std::string filePath = "");

		//加载材质参数
		static void loadMaterialData(std::string filePath = "");

		//获取材质参数原始JSON数据
		static std::string getMaterialJsonStr(std::string filePath = "");



		
		//-----------------------------------------拾取-----------------------------------------------
		static Intersection::Ptr pickInstance(std::vector<float> origin, std::vector<float> direction);
	};
}