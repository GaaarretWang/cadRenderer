# 虚实融合

#### 配置环境
1.  解压flatbuffers-23.5.26和VulkanSceneGraph
2.  cd flatbuffers-23.5.26/build
cmake ..
make
sudo make install
3.  cd VulkanSceneGraph
cmake ..
make
sudo make install
4.  克隆项目代码 https://github.com/GaaarretWang/cadRenderer
5.  dataset3和obj文件夹放在cadRenderer/asset/data目录下

#### 运行代码
cd cadRenderer
bash run.sh