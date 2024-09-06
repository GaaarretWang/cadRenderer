# 虚实融合
#### 配置环境
1.  安装ubuntu系统
2.  在nvidia官网下载版本较新的驱动，参考https://blog.csdn.net/wr1997/article/details/106909423安装
3.  安装vulkansdk
#### 依赖库和数据
1.  解压flatbuffers-23.5.26和VulkanSceneGraph
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
