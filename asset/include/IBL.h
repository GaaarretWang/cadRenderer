#ifndef ZSZ_IBL
#define ZSZ_IBL

#include <vsg/all.h>
#include <vsg/utils/Builder.h>
#include <vsg/utils/ShaderSet.h>
#include "CustomViewDependentState.h"
#include "CustomViewDependentState1.h"

// ============================================
// IBL (Image-Based Lighting) 基于图像的光照系统
// 通过 HDR 环境贴图实现物理真实的环境光照效果
// 包含: 环境贴图(Environment Map)、辐照度(Irradiance)、
//        预滤波环境贴图(Prefiltered Envmap)、BRDF LUT
// ============================================
namespace IBL
{
    template<typename T>
    using ptr = vsg::ref_ptr<T>;// VSG 智能指针简写,类似 std::shared_ptr 但基于 VSG 的引用计数

    // Constants: 各类 IBL 纹理的 GPU 参数常量(format/尺寸/mip级数)
    namespace Constants
    {
        // EnvmapCube: 环境贴图的 cubemap 参数
        // 用 R32G32B32A32_SFLOAT 格式(32位浮点)保存高精度 HDR 数据
        // dim=512, numMips=10 即 log2(512)+1=10 级 mipmap
        namespace EnvmapCube
        {
            constexpr VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;
            constexpr int32_t dim = 512;
            // no auto generated mips
            constexpr uint32_t numMips = 10;  // log2(dim)=9
            //constexpr uint32_t numMips = 1;
            constexpr VkExtent2D extent = {dim, dim};
        } // namespace EnvmapCube
        // BrdfLUT: 双向反射分布函数(BRDF)查找表
        // R16G16_SFLOAT 格式, dim=512, 用于实时查询镜面反射的近似值
        namespace BrdfLUT
        {
            constexpr VkFormat format = VK_FORMAT_R16G16_SFLOAT;
            constexpr int32_t dim = 512;
            constexpr VkExtent2D extent = {dim, dim};
        }
        // IrradianceCube: 漫反射辐照度 cubemap 参数
        // 较低分辨率(dim=64)即可, 因为漫反射光照变化平缓
        // numMips=7 即 floor(log2(64))+1=7 级 mipmap
        namespace IrradianceCube
        {
            constexpr VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;
            constexpr int32_t dim = 64;
            constexpr uint32_t numMips = 7; // floor(log2(dim))) + 1
            constexpr VkExtent2D extent = {dim, dim};
        } // namespace Irradiance
        // PrefilteredEnvmapCube: 镜面反射预滤波环境贴图参数
        // 对不同粗糙度级别进行预卷积, 存储在 mipmap 各级中
        // dim=512, numMips=10, 每级 mip 对应一个粗糙度级别
        namespace PrefilteredEnvmapCube
        {
            constexpr VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;
            constexpr int32_t dim = 512;
            constexpr uint32_t numMips = 10; // log2(dim)=9
            //constexpr int32_t dim = 64;
            //constexpr uint32_t numMips = 7; // floor(log2(dim))) + 1
            constexpr VkExtent2D extent = {dim, dim};
        } // namespace PrefiltedEnvmapCube
    }     // namespace Constants

    // _ImageLine: 单张 HDR 图片对应的 GPU 资源
    // 一张 HDR 图片会生成 cubemap 系列纹理, 每个纹理包含 Image/ImageView/Sampler/ImageInfo
    // 在 VSG 中, Image 是原始图像数据, ImageView 是图像的视图(指定用途和格式),
    // Sampler 定义采样方式(过滤/寻址模式), ImageInfo 把三者绑定在一起供 shader 采样
    struct _ImageLine{
        vsg::ref_ptr<vsg::Image> cube;           // cubemap 原始图像数据(VK image 对象)
        vsg::ref_ptr<vsg::ImageView> cubeView;   // cubemap 视图(指定层数/mip 级等)
        vsg::ref_ptr<vsg::Sampler> cubeSmapler;  // cubemap 采样器(过滤/寻址模式)
        vsg::ref_ptr<vsg::ImageInfo> cubeInfo;   // cubemap 绑定信息(Image+View+Sampler 打包)
    };

    // Textures: IBL 系统的所有 GPU 纹理资源
    // 包含:
    //   - envmapCube: 环境贴图 cubemap (equirectangular→cubemap 转换后的结果)
    //   - testMap/irraMap/prefMap: 按 HDR 图片索引的纹理 map, 支持多张 HDR 切换
    //   - brdfLut: BRDF 查找表, 一次生成永久复用
    //   - irradianceCube: 辐照度 cubemap, 用于漫反射光照
    //   - prefilterCube: 预滤波 cubemap, 用于镜面反射
    //   - params: 传递给 shader 的参数 buffer
    typedef struct _Textures{
        // 环境贴图 cubemap 资源(当前激活的 HDR 对应的 envmap)
        vsg::ref_ptr<vsg::Image> envmapCube;
        vsg::ref_ptr<vsg::ImageView> envmapCubeView;
        vsg::ref_ptr<vsg::Sampler> envmapCubeSmapler;
        vsg::ref_ptr<vsg::ImageInfo> envmapCubeInfo;
        // 按 HDR 图片索引管理的纹理 map, key 为 HDR 图片编号
        std::unordered_map<int, _ImageLine> testMap;   // 环境贴图 cubemap 集合
        std::unordered_map<int, _ImageLine> irraMap;   // 辐照度 cubemap 集合
        std::unordered_map<int, _ImageLine> prefMap;   // 预滤波 cubemap 集合
        // 运行时生成的原始 Image 对象(中间结果, 用于调试导出)
        std::unordered_map<int, vsg::ref_ptr<vsg::Image>> envMap, irradianceMap, prefilteredMap;
        // 共享的 IBL 纹理资源: BRDF LUT / 辐照度 / 预滤波环境贴图
        vsg::ref_ptr<vsg::Image> brdfLut, irradianceCube, prefilterCube;
        vsg::ref_ptr<vsg::ImageView> brdfLutView, irradianceCubeView, prefilterCubeView;
        vsg::ref_ptr<vsg::Sampler> brdfLutSampler, irradianceCubeSampler, prefilterCubeSampler;
        vsg::ref_ptr<vsg::ImageInfo> brdfLutInfo, irradianceCubeInfo, prefilterCubeInfo;

        // IBL 参数 buffer, 传递给 shader 使用(如粗糙度、强度系数等)
        vsg::ref_ptr<vsg::vec4Array> params;
        vsg::ref_ptr<vsg::BufferInfo> paramsInfo;
    } Textures;

    // VsgContext: VSG 渲染上下文, 封装 Vulkan 核心对象
    //   - viewer: VSG 的 Viewer, 管理渲染循环和事件处理
    //   - context: VSG 的 Context, 保存当前帧的 Vulkan 命令缓冲和管线状态
    //   - device: Vulkan 逻辑设备, 所有 GPU 资源和命令的创建入口
    //   - queueFamily: Vulkan 队列族索引, 用于创建 buffer/image 时指定内存位置
    typedef struct _VsgContext
    {
        ptr<vsg::Viewer> viewer;
        //ptr<vsg::Window> window;
        ptr<vsg::Context> context;
        ptr<vsg::Device> device;
        int queueFamily;
    } VsgContext;

    // AppData: 应用层配置数据
    //   - options: VSG 的 Options, 包含搜索路径、文件格式等加载选项
    //   - debugOutputPath: 调试输出目录, 用于导出中间纹理等调试信息
    typedef struct _AppData {
        ptr<vsg::Options> options;
        vsg::Path debugOutputPath;
    } AppData;

    // ---- 全局变量声明 ----
    extern Textures textures;     // 全局 IBL 纹理资源(所有 HDR 共享)
    //extern VsgContext vsgContext;
    extern AppData appData;        // 全局应用配置

    // ---- IBL 资源管理函数 ----

    // 创建 IBL 所需的 GPU 资源(纹理/采样器/缓冲区)
    // hdr_image_max_num: 支持的最大 HDR 图片数量(预分配资源)
    void createResources(VsgContext &vsgContext, int hdr_image_max_num);

    // 加载矩形环境贴图(equirectangular 格式的 HDR 图片到 cubemap 的中间步骤)
    void loadEnvmapRect(VsgContext &vsgContext);

    // 加载 .hdr 文件, 返回浮点像素数据
    // filepath: HDR 文件路径; width/height/channels: 输出图像尺寸和通道数
    // 返回值: 浮点数组指针(RGBE/RGBfloat 格式), 调用者负责释放
    float *loadHdrFile(const std::string &filepath, int &width, int &height, int &channels);

    // 释放所有 IBL GPU 资源(纹理/缓冲区/采样器)
    void clearResources();

    // ---- IBL 纹理生成函数 (基于 compute shader / 渲染到纹理) ----

    // 生成 BRDF LUT (Bidirectional Reflectance Distribution Function Lookup Table)
    // 在 2D 纹理上预计算 Schlick 近似的 BRDF 积分结果, 一次生成永久复用
    void generateBRDFLUT(VsgContext &vsgContext);

    // 生成环境贴图 cubemap: 将 equirectangular HDR 图片投影到 cubemap 的 6 个面
    // envmapFilepath: HDR 图片路径; hdr: HDR 图片索引(用于区分多张 HDR)
    void generateEnvmap(VsgContext& vsgContext, std::string& envmapFilepath, int hdr);

    // 生成辐照度 cubemap: 对环境贴图进行半球积分卷积
    // 输出的是漫反射光照所需的辐照度分布, 分辨率较低(64x64)即可
    void generateIrradianceCube(VsgContext& vsgContext, int hdr);

    // 生成预滤波环境贴图 cubemap: 按粗糙度级别对环境贴图做 GGX 重要性采样卷积
    // 不同 mip 级别存储不同粗糙度的结果, 实现 split-sum 近似中的镜面反射部分
    void generatePrefilteredEnvmapCube(VsgContext& vsgContext, int hdr);

    // 创建天空盒 VSG 节点并添加到场景图
    // context: VSG 渲染上下文; root: 场景根节点 StateGroup
    // width/height: 渲染分辨率; camera_data/depth_data: 相机和深度纹理(用于天空盒采样)
    // shadow_pc_data: 阴影 push constant 数据
    // 返回值: 包含天空盒的 StateGroup 节点
    ptr<vsg::StateGroup> drawSkyboxVSGNode(VsgContext& context, vsg::ref_ptr<vsg::StateGroup> root, int width, int height,  vsg::ImageInfoList camera_data = {}, vsg::ImageInfoList depth_data = {}, vsg::ref_ptr<vsg::Data> shadow_pc_data = {});

    // ---- 自定义 ShaderSet 工厂函数 ----
    // VSG 中 ShaderSet 管理一组 shader 模块(vertex/fragment 等)及其管线配置

    // 创建自定义 PBR (Physically Based Rendering) ShaderSet
    // 基于 VSG 内置 PBR shader, 添加 IBL 环境光照支持
    vsg::ref_ptr<vsg::ShaderSet> customPbrShaderSet(vsg::ref_ptr<const vsg::Options> options);
    // 创建自定义 SSAO (Screen Space Ambient Occlusion) ShaderSet
    // 屏幕空间环境光遮蔽, 用于增强几何细节处的阴影效果
    vsg::ref_ptr<vsg::ShaderSet> customSSAOShaderSet(vsg::ref_ptr<const vsg::Options> options);
    // 创建 SSAO 降噪 ShaderSet, 对 SSAO 原始输出做模糊处理以消除噪声
    vsg::ref_ptr<vsg::ShaderSet> customSSAODenoiseShaderSet(vsg::ref_ptr<const vsg::Options> options);

    // 创建 IBL 演示场景图, 用于测试和调试 IBL 效果
    ptr<vsg::Node> iblDemoSceneGraph(VsgContext& context);
    
    // 更新 HDR 纹理: 切换到指定 HDR 图片对应的 IBL 纹理
    // vsgContext: 渲染命令缓冲(用于插入纹理拷贝/更新命令)
    // hdr: HDR 图片索引, 对应 testMap/irraMap/prefMap 中的 key
    void updateHDRTextures(vsg::ref_ptr<vsg::Commands>& vsgContext, int hdr);
}
#endif // ZSZ_IBL