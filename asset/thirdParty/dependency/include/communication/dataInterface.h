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

	enum ConversionPrecision {
		middle,
		low,
		high
	};

	class DataInterface {

	public: 
		DataInterface& GetInstance() {
			static DataInterface instance;
			return instance;
		}

		//----------------------------------------初始化-----------------------------------------------
		//初始化：读取init.json文件
		static void init();
		//从init.json中获取信息：从本地读取FB文件 or 从云端转换
		static bool isReadLocalFBData();
		//从init.json中获取信息：转换本地CAD文件 or 转换云端CAD文件
		static bool isConvertModelByFile();


		//-----------------------------------------通过本地读取FlatBuffer文件执行模型转换接口：分为init.json读参、直接传参两个版本-----------------------------------------------
		//解析本地CAD（flatBuffer）数据：仅从本地读取时调用
		static void parseLocalModel();
		static void parseLocalModel(std::string fileName, std::string filePath);


		//-----------------------------------------云端通过HTTP请求，执行模型转换接口：分为init.json读参、直接传参两个版本-----------------------------------------------
		//通过HTTP从云端加载： 读取云端路径下的模型
		static void convertModelByPath();
		static void convertModelByPath(std::string host, int port, std::string fileName, std::string filepath, int precision = ConversionPrecision::middle);
		
		//通过HTTP从云端加载： 本地路径下的模型上传
		static void convertModelByFile();
		static void convertModelByFile(std::string host, int port, std::string fileName, std::string filepath, int precision = ConversionPrecision::middle);


		//-----------------------------------------云端建立Socket链接，执行模型转换接口：分为init.json读参、直接传参两个版本-----------------------------------------------
		//与云端建立Socket通信
		static void buildCloudCommunication();
		static void buildCloudCommunication(std::string host, int port);
		
		//通过Socket发送CAD模型数据转换请求
		static void convertModelData();
		static void convertModelData(std::string fileName, std::string filePath, int precision);


		//-----------------------------------------对转换的模型进行解析、管理、编辑、删除-----------------------------------------------

		//CAD模型数据解析：从本地读取的CAD数据或从云端加载的CAD数据，均通过该方法解析，该方法内部自动调用
		static void parseModelData(std::string fileName, char* modelBuffer, size_t modelBufferSize);

		static void setActiveDocumentData(std::string fileName);

		static void removeModelData(std::string fileName);

		//清空当前已解析的所有模型数据
		static void disposeDataManager();



		//-----------------------------------------数据获取接口-----------------------------------------------
		//渲染数据获取接口
		static std::vector<RenderInfo> getRenderInfo(bool printLog = false);

		//pmi接口
		static std::vector<pmiInfo> getPmiInfos(bool printLog = false);

		//instance接口
		static std::unordered_map<std::string, Instance::Ptr> getInstances(bool printLog = false);

		//instanceInfo接口：对instance进行简化，只提供拾取交互必要的信息
		static std::unordered_map<std::string, InstanceInfo::Ptr> getInstanceInfos();

		//获取转换得到的FB数据
		static std::string getModelFlatbuffersData();
	};
}