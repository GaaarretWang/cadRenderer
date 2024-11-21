# 虚实融合
#### 配置环境
1.  安装ubuntu系统
2.  在nvidia官网下载版本较新的驱动，参考https://blog.csdn.net/wr1997/article/details/106909423 安装，驱动版本要在535及以上
3.  安装vulkansdk
4.  安装cuda11.6及以上
#### 依赖库和数据
1.  解压flatbuffers-23.5.26、assimp和VulkanSceneGraph
2.  安装flatbuffer库
```bash  
    cd flatbuffers-23.5.26/build  
    cmake ..  
    make  
    sudo make install  
```
3.  安装VulkanSceneGraph库
```bash  
    cd VulkanSceneGraph
    cmake ..
    make
    sudo make install
```
4.  克隆项目代码 https://github.com/GaaarretWang/cadRenderer
5.  dataset3和obj文件夹放在cadRenderer/asset/data目录下

#### 运行代码
```bash  
    cd cadRenderer
    bash run.sh
```

# 接口文档
## 文件结构简介
根目录中实现本地数据的读取，实现Rendering类和RenderingServer类，模拟服务器与客户端的数据传输，该部分仿照AR引擎中的渲染模块接口进行设计，便于迁移。asset文件夹为渲染器的实现，可以直接替换AR引擎中渲染模块的asset文件夹
## Rendering类和RenderingServer类
#### 控制量
```cpp  
    // 数据集分辨率低，该参数为渲染提升分辨率的倍数
    double upsample_scale = 2; 
    
    // 渲染根目录相对于可执行文件的位置，所有路径都应该相对渲染的根目录来索引
    std::string rendering_dir = "../"; 

    // 每个模型的归一化矩阵，或初始矩阵，与ar引擎对齐
    std::vector<vsg::dmat4> init_model_transforms; 

    // 三个数组的大小应该相等，一一对应
    std::vector<vsg::dmat4> model_transforms; // 可以重复
    std::vector<std::string> model_paths; // 可以重复
    std::vector<std::string> instance_names; // 不可重复

    // true: 在相机跟踪时使用单独的shader，不进行深度剔除，总是显示模型
    // false: 在非相机跟踪时使用深度剔除的shader
    bool trackingShader = true;
```
#### 函数接口
```cpp  
    // 在程序开始调用一次初始化
    int Init();

    // 每次渲染循环都调用
    int Update();
```


