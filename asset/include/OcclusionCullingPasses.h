#include "vsg/all.h"
#include "CADMesh.h"
#include "OffscreenRenderTarget.h"

#ifndef VSGOCCLUSIONCULLINGPASSES_H
#define VSGOCCLUSIONCULLINGPASSES_H

/**
 * OcclusionCullingPasses —— GPU-driven 遮挡剔除模块
 *
 * 实现基于层级深度缓冲 (Hierarchical Z-Buffer, HZB) 的遮挡剔除，整体分为三个 pass：
 *   Pass1 (Frustum Culling):   compute shader 根据视锥体平面剔除不可见实例
 *   Depth Pyramid:             将深度图逐级 2x2 min-downsample 生成 10 级 mipmap
 *   Pass2 (Occlusion Culling): 利用深度金字塔做 hierarchical-Z 遮挡测试
 *
 * VSG 概念速查：
 *   vsg::CommandGraph    —— 一组 GPU 命令的容器，可包含 compute / render pass；
 *                           相当于 Vulkan 的 CommandBuffer + Submit 封装
 *   vsg::ComputePipeline —— 计算管线，绑定 shader + descriptor layout
 *   vsg::BindComputePipeline —— 将 ComputePipeline 绑定到 CommandGraph 上
 *   vsg::Dispatch        —— 发起 compute dispatch，参数为 (groupCountX/Y/Z)
 *   vsg::DescriptorBuffer —— 将 VSG buffer 绑定为 storage buffer 描述符
 *   vsg::DescriptorImage  —— 将 VSG image 绑定为 storage image / combined image sampler 描述符
 *   vsg::PipelineBarrier   —— GPU 管线屏障，用于同步不同 shader 阶段的读写
 *   vsg::BufferMemoryBarrier —— 缓冲区内存屏障，确保 compute 写入后 draw 可读
 *   vsg::ImageMemoryBarrier  —— 图像内存屏障，用于 layout transition 和读写同步
 */
namespace OcclusionCullingPasses{

    /**
     * 视锥体平面信息 —— 存储 6 个归一化平面方程 (near, far, left, right, bottom, top)
     * 在 compute shader 中用 dot(plane, worldPos) 判断点是否在视锥内
     */
    struct CameraPlaneInfo{
        vsg::vec4 n[6];  // 每个平面: (normal.x, normal.y, normal.z, distance)
    };

    /**
     * Compute shader push constants —— 传递屏幕分辨率
     * 用于 shader 中将像素坐标转换为 NDC 或计算 dispatch 大小
     */
    struct ComputePushConstants {
        uint32_t width;
        uint32_t height;
        char padding[8];  // 对齐到 16 字节（Vulkan 要求 push constant 为 16 字节对齐）
    };

    // ==================== Depth Pyramid (层级深度图) 相关资源 ====================

    extern int mip_level_count;                                   // 深度金字塔 mipmap 层数，固定为 10
    extern vsg::ref_ptr<vsg::Image> depthPyramidImage;            // 深度金字塔 VkImage (R32_SFLOAT 格式，10 层 mipmap)
    extern vsg::ref_ptr<vsg::Sampler> depth_pyramid_sampler;      // 采样器，NEAREST 过滤，用于读取深度金字塔
    extern vsg::ref_ptr<vsg::ImageView> depthPyramidImageView;    // 深度金字塔的 image view，覆盖所有 mip level
    extern vsg::ref_ptr<vsg::ImageInfo> depthPyramidImageInfo;    // sampler + imageView 组合，绑定到 descriptor set
    extern vsg::ref_ptr<vsg::ImageInfo> framebuffer_depthImageInfo; // 帧缓冲深度图 image view，用于 Pass1 拷贝到 pyramid level 0

    /**
     * 初始化深度金字塔的 Image、Sampler、ImageView 和 ImageInfo
     * @param extent        渲染分辨率（宽度和高度）
     * @param offscreenTarget 离屏渲染目标，提供原始深度 image view
     */
    void initOcclusionCullingPassesImageInfo(VkExtent2D extent, vsg::ref_ptr<OffscreenRenderTarget> offscreenTarget);

    // ==================== Camera (相机) 相关数据 ====================

    extern CameraPlaneInfo camera_plane_info;                                       // 视锥体 6 个平面的方程
    extern vsg::ref_ptr<vsg::Array<CameraPlaneInfo>> camera_plane_info_buffer;      // GPU buffer，存储平面数据供 shader 读取
    extern vsg::ref_ptr<vsg::BufferInfo> camera_plane_info_buffer_info;             // buffer 的描述信息，用于 descriptor 绑定
    extern vsg::ref_ptr<vsg::mat4Array> camera_matrix;                              // 存储 [viewMatrix, viewProjectionMatrix] 两个矩阵

    /**
     * 根据相机内参和外参生成视锥体平面 + view/projection 矩阵
     * @param fx,fy  焦距（像素单位）
     * @param cx,cy  主点坐标
     * @param w,h    图像分辨率
     * @param near,far 近/远裁剪面距离
     * @param camera VSG Camera 对象（用于获取 viewMatrix / projectionMatrix）
     */
    void generateCameraData(double fx, double fy, double cx, double cy, double w, double h, double near, double far, vsg::ref_ptr<vsg::Camera> camera);

    // ==================== 三个核心 Pass ====================

    /**
     * Pass1: 视锥体剔除 (Frustum Culling)
     * Compute shader 逐实例检查 AABB 是否与视锥体相交，通过则保留间接绘制命令
     * @param depth_cull_command_graph1 用于记录 compute 命令的 CommandGraph
     * @param options  VSG 资源查找路径
     */
    void buildFirstComputePass(vsg::ref_ptr<vsg::CommandGraph> depth_cull_command_graph1, vsg::ref_ptr<vsg::Options> options);

    /**
     * Depth Pyramid 构建: 10 级 min-downsample
     * Level 0: 从原始深度图拷贝
     * Level 1-9: 每级从上一级做 2x2 min-downsample
     * @param depth_pyramid_CommandGraph CommandGraph
     * @param options   VSG 资源查找路径
     * @param extent    渲染分辨率
     * @param offscreenTarget 离屏渲染目标（提供原始深度图）
     */
    void buildDepthPyramid(vsg::ref_ptr<vsg::CommandGraph> depth_pyramid_CommandGraph, vsg::ref_ptr<vsg::Options> options, VkExtent2D extent, vsg::ref_ptr<OffscreenRenderTarget> offscreenTarget);

    /**
     * Pass2: 深度遮挡剔除 (Depth Occlusion Culling)
     * 利用 depth pyramid 做 hierarchical-Z 测试，剔除被近处物体完全遮挡的实例
     * 包含两个 shader 变体：
     *   computevertex1.comp      — 大实例 (instance_count > 32)，每个 workgroup 处理 700 个
     *   computevertex1_seat.comp — 小实例 (instance_count <= 32)，每个 workgroup 处理 32 个
     * @param depth_pyramid_CommandGraph CommandGraph
     * @param options   VSG 资源查找路径
     * @param extent    渲染分辨率
     */
    void buildSecondComputePass(vsg::ref_ptr<vsg::CommandGraph> depth_pyramid_CommandGraph, vsg::ref_ptr<vsg::Options> options, VkExtent2D extent);

}
#endif