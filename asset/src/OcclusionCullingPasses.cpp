#include "OcclusionCullingPasses.h"

namespace OcclusionCullingPasses{
    // ==================== 深度金字塔全局资源 ====================
    int mip_level_count = 10;  // 层级深度图共 10 级 mipmap（level 0~9）
    vsg::ref_ptr<vsg::Image> depthPyramidImage;
    vsg::ref_ptr<vsg::Sampler> depth_pyramid_sampler;
    vsg::ref_ptr<vsg::ImageView> depthPyramidImageView;
    vsg::ref_ptr<vsg::ImageInfo> depthPyramidImageInfo;
    vsg::ref_ptr<vsg::ImageInfo> framebuffer_depthImageInfo;  // 帧缓冲原始深度图的 image view

    /**
     * 创建深度金字塔所需的 Image / Sampler / ImageView / ImageInfo
     *
     * 深度金字塔是一张 R32_SFLOAT 格式的 2D 图像，拥有 10 级 mipmap，
     * 每级 mipmap 的宽高是上一级的一半，存储该区域的最小深度值。
     * Compute shader 通过 storage image 读写这张图。
     *
     * VSG 概念:
     *   vsg::Image      —— 对应 VkImage，定义格式、尺寸、usage flags
     *   vsg::Sampler    —— 对应 VkSampler，控制纹理采样方式（过滤、mipmap 模式）
     *   vsg::ImageView  —— 对应 VkImageView，定义 Image 的哪些层/mip 可被访问
     *   vsg::ImageInfo   —— VSG 的高层封装，组合 sampler + imageView + layout，方便绑定到 descriptor
     */
    void initOcclusionCullingPassesImageInfo(VkExtent2D extent, vsg::ref_ptr<OffscreenRenderTarget> offscreenTarget){
        // ---- 创建 Image：R32_SFLOAT 格式的 2D 图像，10 级 mipmap ----
        // 使用 R32_SFLOAT 而非深度格式，这样 compute shader 可以用 storage image 读写
        depthPyramidImage = vsg::Image::create();
        depthPyramidImage->imageType = VK_IMAGE_TYPE_2D;
        depthPyramidImage->format = VK_FORMAT_R32_SFLOAT;  // 单通道 32 位浮点，适合存储最小深度值
        depthPyramidImage->mipLevels = 10;                 // 共 10 级 mipmap (level 0~9)
        depthPyramidImage->arrayLayers = 1;                // 单层，不需要 array
        depthPyramidImage->usage = VK_IMAGE_USAGE_STORAGE_BIT |          // compute shader 读写 (storage image)
                                    VK_IMAGE_USAGE_SAMPLED_BIT |          // 可被采样 (combined image sampler)
                                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT |     // 可作为拷贝源
                                    VK_IMAGE_USAGE_TRANSFER_DST_BIT;      // 可作为拷贝目标
        depthPyramidImage->initialLayout = VK_IMAGE_LAYOUT_GENERAL;      // 初始即为 GENERAL 布局（允许 compute 读写）
        depthPyramidImage->extent.width = extent.width;    // 与渲染分辨率一致
        depthPyramidImage->extent.height = extent.height;
        depthPyramidImage->extent.depth = 1;

        // ---- 创建 Sampler：NEAREST 过滤，确保取到精确的最小深度值 ----
        depth_pyramid_sampler = vsg::Sampler::create();
        depth_pyramid_sampler->minLod = 0;
        depth_pyramid_sampler->maxLod = static_cast<uint32_t>(std::max(0, mip_level_count - 1));  // maxLod = 9
        depth_pyramid_sampler->magFilter = VK_FILTER_NEAREST;  // 放大时不插值
        depth_pyramid_sampler->minFilter = VK_FILTER_NEAREST;  // 缩小时不插值（取精确最小值）
        depth_pyramid_sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST; // mipmap 选择最近级别

        // ---- 创建 ImageView：覆盖全部 10 级 mipmap ----
        depthPyramidImageView = vsg::ImageView::create(depthPyramidImage);
        depthPyramidImageView->subresourceRange.baseMipLevel = 0;
        depthPyramidImageView->subresourceRange.levelCount = mip_level_count;

        // ---- 组装 ImageInfo：sampler + imageView + layout，供 descriptor 绑定 ----
        depthPyramidImageInfo = vsg::ImageInfo::create(depth_pyramid_sampler, depthPyramidImageView, VK_IMAGE_LAYOUT_GENERAL);

        // framebuffer_depthImageInfo 用于 depth pyramid level 0 的拷贝源
        // offscreenTarget->depthImageView 是离屏渲染的原始深度 attachment
        framebuffer_depthImageInfo = vsg::ImageInfo::create(depth_pyramid_sampler, offscreenTarget->depthImageView);
    }

    // ==================== Camera 相关全局变量 ====================
    CameraPlaneInfo camera_plane_info;                                      // CPU 端存储的 6 个视锥体平面
    vsg::ref_ptr<vsg::Array<CameraPlaneInfo>> camera_plane_info_buffer;     // GPU buffer，存储平面数据
    vsg::ref_ptr<vsg::BufferInfo> camera_plane_info_buffer_info;            // buffer 的描述信息，用于 descriptor 绑定

    vsg::ref_ptr<vsg::mat4Array> camera_matrix = vsg::mat4Array::create(2); // 存储 2 个 mat4: [0]=viewMatrix, [1]=viewProjectionMatrix
    vsg::ref_ptr<vsg::BufferInfo> camera_matrix_buffer_info;

    /**
     * 根据相机内参 (fx, fy, cx, cy) 和外参 (viewMatrix, projectionMatrix) 计算视锥体平面
     *
     * 视锥体 6 个平面的顺序: [0]near [1]far [2]left [3]right [4]bottom [5]top
     * 每个平面用齐次方程 ax+by+cz+d=0 表示，即 vec4(a,b,c,d)
     * shader 中判断可见性: dot(plane, vec4(worldPos, 1.0)) >= 0 表示在平面内侧
     *
     * 同时存储 viewMatrix 和 viewProjectionMatrix 到 GPU buffer，
     * 供 compute shader 做 AABB 的世界空间到裁剪空间变换
     */
    void generateCameraData(double fx, double fy, double cx, double cy, double w, double h, double near, double far, vsg::ref_ptr<vsg::Camera> camera){
        // ---- 计算 6 个视锥体平面方程 ----
        // 每个平面由内参 (fx, fy, cx, cy) 和图像分辨率推导
        // 这些平面位于 view space，shader 中将 AABB 的包围球中心变换到 view space 后做测试
        camera_plane_info.n[0] = vsg::normalize(vsg::vec4(0, 0, -1, -near));          // Near 平面: z >= -near
        camera_plane_info.n[1] = vsg::normalize(vsg::vec4(0, 0, 1, far));              // Far 平面:  z <= far
        camera_plane_info.n[2] = vsg::normalize(vsg::vec4(2*fx/w, 0, -2*cx/w, 0));    // Left 平面
        camera_plane_info.n[3] = vsg::normalize(vsg::vec4(-2*fx/w, 0, -2+2*cx/w, 0)); // Right 平面
        camera_plane_info.n[4] = vsg::normalize(vsg::vec4(0, 2*fy/h, -2+2*cy/h, 0));  // Bottom 平面
        camera_plane_info.n[5] = vsg::normalize(vsg::vec4(0, -2*fy/h, -2*cy/h, 0));   // Top 平面

        // ---- 将平面数据上传到 GPU buffer ----
        // vsg::Array 是 VSG 中的数组类型，可直接作为 storage buffer 使用
        camera_plane_info_buffer = vsg::Array<CameraPlaneInfo>::create(1);
        camera_plane_info_buffer->set(0, camera_plane_info);

        // vsg::BufferInfo 描述 buffer 的元信息（偏移、范围等），是 descriptor 绑定的桥梁
        camera_plane_info_buffer_info = vsg::BufferInfo::create(camera_plane_info_buffer);

        // ---- 存储 view / viewProjection 矩阵 ----
        // camera_matrix[0] = viewMatrix (世界空间 -> 相机空间)
        // camera_matrix[1] = projectionMatrix * viewMatrix (世界空间 -> 裁剪空间)
        camera_matrix->set(0, vsg::mat4(camera->viewMatrix->transform()));
        camera_matrix->set(1, vsg::mat4(camera->projectionMatrix->transform() * camera->viewMatrix->transform()));
        camera_matrix->properties.dataVariance = vsg::DYNAMIC_DATA;  // 标记为动态数据，VSG 会在每帧更新

        camera_matrix_buffer_info = vsg::BufferInfo::create(camera_matrix);
    }

    /**
     * Pass1: Frustum Culling（视锥体剔除）
     *
     * 构建流程：
     *   1. 插入 barrier: DRAW_INDIRECT -> COMPUTE_SHADER
     *      确保之前的间接绘制命令已写入，compute shader 可安全读写
     *   2. 创建 compute pipeline 并绑定
     *   3. 遍历每个 proto（原型对象），绑定 descriptor set，dispatch compute
     *   4. 插入 barrier: COMPUTE_SHADER -> DRAW_INDIRECT
     *      确保 compute shader 写入完成后，后续渲染可安全间接绘制
     *
     * Descriptor 布局 (binding 0~11):
     *   0: draw_indirect          间接绘制命令缓冲（compute 读写）
     *   1: indirect_full          完整间接绘制命令（备份）
     *   2: input_instance         输入实例数据
     *   3: input_highlight        高亮数据
     *   4: output_instance        输出实例数据（剔除后的结果）
     *   5: camera_plane_info      视锥体 6 个平面方程
     *   6: bounds                 每个实例的 AABB 包围盒
     *   7: camera_matrix          view / viewProjection 矩阵
     *   8: (combined image sampler) 未在此 pass 使用
     *   9: global_model_matrix    全局模型变换矩阵
     *   10: last_global_model_matrix 上一帧的全局模型变换矩阵
     *   11: last_instance         上一帧的实例数据
     */
    void buildFirstComputePass(vsg::ref_ptr<vsg::CommandGraph> depth_cull_command_graph1, vsg::ref_ptr<vsg::Options> options)
    {
        // ---- 定义 descriptor set layout：12 个 binding，大部分是 storage buffer ----
        vsg::DescriptorSetLayoutBindings descriptorBindings{
            {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {8, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {9, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {10, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {11, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        };
        auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);
        auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, vsg::PushConstantRanges{});  // 此 pass 无 push constants

        // ---- Barrier 1: 间接绘制 -> 计算着色器 ----
        // 上一帧（可能是 shadow pass）可能还在读取间接绘制命令
        // 我们需要等它读完，再让 compute shader 写入
        // VSG 概念: PipelineBarrier 对应 vkCmdPipelineBarrier，指定源/目标管线阶段
        auto ShadowToPass1CullBarrier = vsg::PipelineBarrier::create(
            VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0
        );
        for(auto& proto_data_itr : CADMesh::proto_id_to_data_map){
            ProtoData* proto_data = proto_data_itr.second;
            // BufferMemoryBarrier: 针对单个 buffer 的内存屏障
            // 等待 INDIRECT_COMMAND_READ 完成后，允许 SHADER_WRITE
            auto indirectBarrier = vsg::BufferMemoryBarrier::create(
                VK_ACCESS_INDIRECT_COMMAND_READ_BIT,  // 源: 之前的间接绘制读取
                VK_ACCESS_SHADER_WRITE_BIT,            // 目标: compute shader 写入
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                proto_data->draw_indirect->bufferInfo->buffer,  // 目标 buffer: 间接绘制命令
                0,
                VK_WHOLE_SIZE
            );
            ShadowToPass1CullBarrier->add(indirectBarrier);  // 将 barrier 加入管线屏障
        }
        depth_cull_command_graph1->addChild(ShadowToPass1CullBarrier);  // 将屏障添加到命令图

        // ---- Barrier 2 预创建（将在所有 proto 的 compute 之后添加）----
        // compute shader 写完间接绘制命令后，后续渲染阶段才能读取
        auto Pass1CullToPass1Barrier = vsg::PipelineBarrier::create(
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,   // 源: compute shader 写入
            VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,    // 目标: 间接绘制读取
            0
        );

        // ---- 创建 compute pipeline ----
        // VSG 概念: ComputePipeline = PipelineLayout + ShaderStage
        //   PipelineLayout = DescriptorSetLayout(s) + PushConstantRange(s)
        //   BindComputePipeline 将管线绑定到 CommandGraph，后续 dispatch 都用这个管线
        auto shaderPath = vsg::findFile("shaders/computevertex.comp", options->paths);
        auto computeShader = vsg::read_cast<vsg::ShaderStage>(shaderPath, options);
        auto pipeline = vsg::ComputePipeline::create(pipelineLayout, computeShader);
        auto bindPipeline = vsg::BindComputePipeline::create(pipeline);
        depth_cull_command_graph1->addChild(bindPipeline);  // 绑定管线

        // ---- 逐 proto 绑定 descriptor 并 dispatch ----
        for(auto& proto_data_itr : CADMesh::proto_id_to_data_map){
            ProtoData* proto_data = proto_data_itr.second;
            // DescriptorBuffer: 将 VSG BufferInfo 绑定到 descriptor set 的指定 binding
            // binding 0~7: storage buffers（间接绘制命令、实例数据、包围盒、矩阵等）
            auto storageBuffer = vsg::DescriptorBuffer::create(vsg::BufferInfoList{proto_data->draw_indirect->bufferInfo, proto_data->indirect_full_buffer_info,
                                                                                proto_data->input_instance_buffer_info, proto_data->input_highlight_buffer_info,
                                                                                proto_data->output_instance_buffer_info, camera_plane_info_buffer_info,
                                                                                proto_data->bounds_buffer_info, camera_matrix_buffer_info}, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            // binding 9: 全局模型矩阵（所有实例共享的变换）
            auto globalModelBuffer = vsg::DescriptorBuffer::create(vsg::BufferInfoList{CADMesh::global_model_matrix_buffer_info}, 9, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            // binding 10: 上一帧的全局模型矩阵（用于运动检测等）
            auto lastGlobalModelBuffer = vsg::DescriptorBuffer::create(vsg::BufferInfoList{CADMesh::last_global_model_matrix_buffer_info}, 10, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            // binding 11: 上一帧的实例数据
            auto lastProtoBuffer = vsg::DescriptorBuffer::create(vsg::BufferInfoList{proto_data->last_instance_buffer_info}, 11, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            // 创建 descriptor set 并绑定到管线
            auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{storageBuffer, globalModelBuffer, lastGlobalModelBuffer, lastProtoBuffer});
            auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, descriptorSet);
            depth_cull_command_graph1->addChild(bindDescriptorSet);

            // Dispatch: 发起 compute dispatch
            // 每个 workgroup 处理 700 个实例，groupCount = instance_count / 700 + 1
            // (X, 1, 1) 表示一维 dispatch，shader 内通过 gl_GlobalInvocationID.x 获取实例索引
            depth_cull_command_graph1->addChild(vsg::Dispatch::create(proto_data->instance_matrix.size() / 700 +  1, 1, 1));

            // 收集每个 proto 的 indirect barrier，等所有 proto 的 compute 完成后一起 barrier
            auto indirectBarrier = vsg::BufferMemoryBarrier::create(
                VK_ACCESS_SHADER_WRITE_BIT,
                VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                proto_data->draw_indirect->bufferInfo->buffer,
                0,
                VK_WHOLE_SIZE
            );
            Pass1CullToPass1Barrier->add(indirectBarrier);  // 收集 barrier
        }
        depth_cull_command_graph1->addChild(Pass1CullToPass1Barrier);  // 最后插入 barrier，确保 compute 写入完成后才可间接绘制
    }

    /**
     * Depth Pyramid 构建: 10 级 min-downsample 深度金字塔
     *
     * 构建流程:
     *   1. 插入 image barrier: 将原始深度图从 DEPTH_STENCIL_ATTACHMENT 转为 SHADER_READ_ONLY
     *   2. Level 0: 用 computevertex_depthimage.comp 从原始深度图拷贝到 pyramid level 0
     *   3. Level 1-9: 用 computevertex_depthpyramid.comp 从前一级做 2x2 min-downsample
     *   4. 每级之间插入 ImageMemoryBarrier 确保写入完成后下一级可读
     *   5. 最终 barrier: 将整张金字塔的 compute write 同步为后续 pass 的 compute read
     *
     * Descriptor 布局:
     *   binding 0: storage image — 写入目标 (当前 mip level)
     *   binding 1: storage image — 读取来源 (上一级 mip level)
     *
     * Push Constants:
     *   width, height — 当前 mip level 的分辨率
     */
    void buildDepthPyramid(vsg::ref_ptr<vsg::CommandGraph> depth_pyramid_CommandGraph, vsg::ref_ptr<vsg::Options> options, VkExtent2D extent, vsg::ref_ptr<OffscreenRenderTarget> offscreenTarget)
    {
        // ---- 定义 descriptor layout: 2 个 storage image binding ----
        // binding 0: 写入目标 (当前 mip level)
        // binding 1: 读取来源 (上一级 mip level)
        vsg::DescriptorSetLayoutBindings descriptorBindings{
            {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        };
        auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);
        // PipelineLayout: 描述符布局 + push constant 范围
        // Push constants 传递当前 mip level 的分辨率 (width, height)
        auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout},
                vsg::PushConstantRanges{
                    {VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants)}  // 传递 width, height
                });
        {
            // ========== Level 0: 从原始深度图拷贝到 depth pyramid level 0 ==========
            // ImageMemoryBarrier: 同步 depthPyramidImage 的读写
            // 确保之前对金字塔的读写完成，level 0 的 compute shader 可安全写入
            auto barrier = vsg::ImageMemoryBarrier::create(
                VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,  // 源: 之前的读写
                VK_ACCESS_SHADER_WRITE_BIT,                              // 目标: compute 写入
                VK_IMAGE_LAYOUT_GENERAL,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                depthPyramidImage,
                VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 10, 0, 1}  // 全部 10 个 mip level
            );

            // 将原始深度图从 DEPTH_STENCIL_ATTACHMENT 布局转为 SHADER_READ_ONLY
            // 这样 compute shader 就能读取深度值
            auto depthToComputeBarrier = vsg::ImageMemoryBarrier::create(
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,   // 源: 深度附件写入完成
                VK_ACCESS_SHADER_READ_BIT,                      // 目标: shader 可读取
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,  // 源布局: 深度附件
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,          // 目标布局: shader 只读
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                offscreenTarget->depthImage,                     // 原始深度图
                VkImageSubresourceRange{VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1}  // 仅深度 aspect
            );

            // 将两个 barrier 一起插入管线
            depth_pyramid_CommandGraph->addChild(vsg::PipelineBarrier::create(
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // 源阶段
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // 目标阶段
                0, barrier, depthToComputeBarrier
            ));

            // ---- 创建 Level 0 的 compute pipeline ----
            // computevertex_depthimage.comp: 从原始深度图读取，写入 pyramid level 0
            auto shaderPath = vsg::findFile("shaders/computevertex_depthimage.comp", options->paths);
            auto computeShader = vsg::read_cast<vsg::ShaderStage>(shaderPath, options);
            auto pipeline = vsg::ComputePipeline::create(pipelineLayout, computeShader);
            auto bindPipeline = vsg::BindComputePipeline::create(pipeline);
            depth_pyramid_CommandGraph->addChild(bindPipeline);

            // Push constants: 传递全分辨率 (width, height) 给 level 0 的 compute shader
            auto pcData = vsg::Value<ComputePushConstants>::create(ComputePushConstants{extent.width, extent.height});
            depth_pyramid_CommandGraph->addChild(vsg::PushConstants::create(
                VK_SHADER_STAGE_COMPUTE_BIT,
                0,
                pcData
            ));

            // DescriptorImage: 将 depth pyramid (写入目标) 和 framebuffer depth (读取来源) 绑定
            // binding 0: depthPyramidImageInfo (所有 mip levels 的 image view)
            // binding 1: framebuffer_depthImageInfo (原始深度图的 image view)
            auto storageImage = vsg::DescriptorImage::create(vsg::ImageInfoList{depthPyramidImageInfo, framebuffer_depthImageInfo}, 0);
            auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{storageImage});
            auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, descriptorSet);
            depth_pyramid_CommandGraph->addChild(bindDescriptorSet);

            // Dispatch level 0: 32x32 个线程一组，覆盖全分辨率
            depth_pyramid_CommandGraph->addChild(vsg::Dispatch::create((extent.width + 31) / 32, (extent.height + 31) / 32, 1));

            // Level 0 写入完成后插入 barrier，确保 level 1 读取时数据已就绪
            // barrier1: 仅同步 level 0，等待其写入完成
            auto barrier1 = vsg::ImageMemoryBarrier::create(
                VK_ACCESS_SHADER_WRITE_BIT,   // 源: level 0 的 compute 写入
                VK_ACCESS_SHADER_READ_BIT,    // 目标: level 1 的 compute 读取
                VK_IMAGE_LAYOUT_GENERAL,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                depthPyramidImage,
                VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}  // 仅 level 0
            );

            depth_pyramid_CommandGraph->addChild(vsg::PipelineBarrier::create(
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // 源: level 0 compute
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // 目标: level 1 compute
                0, barrier1
            ));
        }

        // ========== Level 1-9: 2x2 min-downsample 逐级生成 ==========
        // 每一级从上一级读取，对 2x2 像素取最小值写入当前级
        for(uint32_t i = 1; i < 10; i ++)
        {
            // computevertex_depthpyramid.comp: 对上一级做 2x2 min-downsample
            auto shaderPath = vsg::findFile("shaders/computevertex_depthpyramid.comp", options->paths);
            auto computeShader = vsg::read_cast<vsg::ShaderStage>(shaderPath, options);
            auto pipeline = vsg::ComputePipeline::create(pipelineLayout, computeShader);
            auto bindPipeline = vsg::BindComputePipeline::create(pipeline);
            depth_pyramid_CommandGraph->addChild(bindPipeline);

            // Push constants: 当前 mip level 的分辨率 = 原始分辨率 >> i
            auto pcData = vsg::Value<ComputePushConstants>::create(ComputePushConstants{extent.width >> i, extent.height >> i});
            depth_pyramid_CommandGraph->addChild(vsg::PushConstants::create(
                VK_SHADER_STAGE_COMPUTE_BIT,
                0,
                pcData
            ));

            // 为当前 mip level (i) 创建独立的 ImageView，仅覆盖这一级
            // VSG 概念: 需要为每个 mip level 创建单独的 ImageView 才能作为 storage image 使用
            auto i_image_view = vsg::ImageView::create(depthPyramidImage);
            i_image_view->subresourceRange.baseMipLevel = i;    // 当前级
            i_image_view->subresourceRange.levelCount = 1;       // 仅 1 级
            auto i_depthPyramidImageInfo = vsg::ImageInfo::create(depth_pyramid_sampler, i_image_view, VK_IMAGE_LAYOUT_GENERAL);

            // 为上一级 (i-1) 创建 ImageView，作为读取来源
            auto i1_image_view = vsg::ImageView::create(depthPyramidImage);
            i1_image_view->subresourceRange.baseMipLevel = i - 1;  // 上一级
            i1_image_view->subresourceRange.levelCount = 1;
            auto i1_depthPyramidImageInfo = vsg::ImageInfo::create(depth_pyramid_sampler, i1_image_view, VK_IMAGE_LAYOUT_GENERAL);

            // 绑定: binding 0 = 当前级 (写入), binding 1 = 上一级 (读取)
            auto storageImage0 = vsg::DescriptorImage::create(vsg::ImageInfoList{i_depthPyramidImageInfo}, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
            auto storageImage1 = vsg::DescriptorImage::create(vsg::ImageInfoList{i1_depthPyramidImageInfo}, 1, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
            auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{storageImage0, storageImage1});
            auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, descriptorSet);
            depth_pyramid_CommandGraph->addChild(bindDescriptorSet);

            // Dispatch: 按当前 mip 的分辨率计算 workgroup 数量
            uint32_t mipWidth = std::max(1u, extent.width >> i);
            uint32_t mipHeight = std::max(1u, extent.height >> i);
            depth_pyramid_CommandGraph->addChild(vsg::Dispatch::create(
                (mipWidth + 31) / 32,
                (mipHeight + 31) / 32,
                1
            ));

            // 每级写入后插入 barrier，确保下一级读取时数据已就绪
            auto barrier = vsg::ImageMemoryBarrier::create(
                VK_ACCESS_SHADER_WRITE_BIT,   // 源: 当前级 compute 写入
                VK_ACCESS_SHADER_READ_BIT,    // 目标: 下一级 compute 读取
                VK_IMAGE_LAYOUT_GENERAL,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                depthPyramidImage,
                VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, i, 1, 0, 1}  // 仅同步当前 mip level i
            );

            depth_pyramid_CommandGraph->addChild(vsg::PipelineBarrier::create(
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0, barrier
            ));
        }

        // ========== 最终 barrier: 金字塔生成完成 -> Pass2 剔除可读 ==========
        // 同步全部 10 个 mip level 的写入，确保后续 Pass2 的 compute shader 读取正确
        auto pyramidFinalBarrier = vsg::ImageMemoryBarrier::create(
            VK_ACCESS_SHADER_WRITE_BIT,          // 源: 金字塔生成的 compute 写入
            VK_ACCESS_SHADER_READ_BIT,           // 目标: Pass2 剔除阶段的 compute 读取
            VK_IMAGE_LAYOUT_GENERAL,             // 源布局: GENERAL（compute 读写通用）
            VK_IMAGE_LAYOUT_GENERAL,             // 目标布局: GENERAL（保持不变）
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            depthPyramidImage,
            VkImageSubresourceRange{
                VK_IMAGE_ASPECT_COLOR_BIT,       // 注意: depthPyramidImage 是 R32_SFLOAT 格式
                                                 // 它不是深度格式，所以用 COLOR_BIT 而非 DEPTH_BIT
                0, 10, 0, 1                       // 覆盖全部 10 个 mip level (0~9)
            }
        );

        depth_pyramid_CommandGraph->addChild(vsg::PipelineBarrier::create(
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // 源阶段: 金字塔生成
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // 目标阶段: Pass2 剔除
            0,
            pyramidFinalBarrier
        ));
    }

    /**
     * Pass2: Depth Occlusion Culling（深度遮挡剔除）
     *
     * 利用 depth pyramid 做 hierarchical-Z 测试:
     *   - 将实例 AABB 投影到屏幕空间，找到覆盖的 mip level
     *   - 采样该 mip level 的最小深度值
     *   - 如果实例最近深度 > 采样值，说明被完全遮挡，剔除
     *
     * 包含两个 shader 变体:
     *   computevertex1.comp      — 大实例 (instance_count > 32)，workgroup size = 700
     *   computevertex1_seat.comp — 小实例 (instance_count <= 32)，workgroup size = 32
     *
     * 当 proto 之间实例大小交替时，需要切换 pipeline
     *
     * Descriptor 布局与 Pass1 类似，额外增加了:
     *   binding 8: depth pyramid (combined image sampler) — 用于遮挡测试
     */
    void buildSecondComputePass(vsg::ref_ptr<vsg::CommandGraph> depth_pyramid_CommandGraph, vsg::ref_ptr<vsg::Options> options, VkExtent2D extent)
        {
        // ---- Descriptor layout: 12 个 binding，与 Pass1 相同，额外 binding 8 = depth pyramid ----
        vsg::DescriptorSetLayoutBindings descriptorBindings{
            {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {8, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {9, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {10, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {11, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        };
        auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);
        // PipelineLayout 带 push constants: 传递屏幕分辨率给 shader
        auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout},
                vsg::PushConstantRanges{
                    {VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants)}  // width, height
                });

        // ---- Barrier 预创建: compute shader -> 间接绘制 ----
        // Pass2 完成后，间接绘制命令已被更新，需要 barrier 确保可读
        auto Pass2CullToPass2Barrier = vsg::PipelineBarrier::create(
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
            0
        );
        // ---- 创建两个 shader 变体的 compute pipeline ----
        // 变体 1: computevertex1.comp — 大实例，workgroup 处理 700 个实例
        auto shaderPath = vsg::findFile("shaders/computevertex1.comp", options->paths);
        auto computeShader = vsg::read_cast<vsg::ShaderStage>(shaderPath, options);
        auto pipeline = vsg::ComputePipeline::create(pipelineLayout, computeShader);
        auto bindPipeline = vsg::BindComputePipeline::create(pipeline);

        // 变体 2: computevertex1_seat.comp — 小实例 (<=32)，workgroup 处理 32 个实例
        auto shaderPath_seat = vsg::findFile("shaders/computevertex1_seat.comp", options->paths);
        auto computeShader_seat = vsg::read_cast<vsg::ShaderStage>(shaderPath_seat, options);
        auto pipeline_seat = vsg::ComputePipeline::create(pipelineLayout, computeShader_seat);
        auto bindPipeline_seat = vsg::BindComputePipeline::create(pipeline_seat);

        // 默认先绑定大实例的 pipeline
        depth_pyramid_CommandGraph->addChild(bindPipeline);

        // Push constants: 传递屏幕分辨率
        auto pcData1 = vsg::Value<ComputePushConstants>::create(ComputePushConstants{extent.width, extent.height});
        depth_pyramid_CommandGraph->addChild(vsg::PushConstants::create(
            VK_SHADER_STAGE_COMPUTE_BIT,
            0,
            pcData1
        ));

        // ---- 逐 proto 绑定 descriptor、切换 pipeline 并 dispatch ----
        // pre_pipeline 记录当前绑定的 pipeline，避免重复绑定
        auto pre_pipeline = bindPipeline;
        for(auto& proto_data_itr : CADMesh::proto_id_to_data_map){
            ProtoData* proto_data = proto_data_itr.second;

            // 根据实例数量切换 shader 变体:
            //   > 32 个实例: 使用 computevertex1_seat.comp (workgroup 处理 32 个)
            //   <= 32 个实例: 使用 computevertex1.comp (workgroup 处理 700 个)
            if(proto_data->instance_matrix.size() > 32 && pre_pipeline == bindPipeline){
                // 当前是大实例 pipeline，需要切到小实例 pipeline
                depth_pyramid_CommandGraph->addChild(bindPipeline_seat);
                pre_pipeline = bindPipeline_seat;
            }
            else if(proto_data->instance_matrix.size() <= 32 && pre_pipeline == bindPipeline_seat){
                // 当前是小实例 pipeline，需要切回大实例 pipeline
                depth_pyramid_CommandGraph->addChild(bindPipeline);
                pre_pipeline = bindPipeline;
            }

            // 绑定 storage buffers (binding 0~7): 间接绘制命令、实例数据、矩阵等
            auto storageBuffer = vsg::DescriptorBuffer::create(vsg::BufferInfoList{proto_data->draw_indirect->bufferInfo, proto_data->indirect_full_buffer_info,
                                                                                proto_data->input_instance_buffer_info, proto_data->input_highlight_buffer_info,
                                                                                proto_data->output_instance_buffer_info, camera_plane_info_buffer_info,
                                                                                proto_data->bounds_buffer_info, camera_matrix_buffer_info}, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            // binding 8: depth pyramid (combined image sampler)，用于遮挡测试
            auto storageImage = vsg::DescriptorImage::create(vsg::ImageInfoList{depthPyramidImageInfo}, 8);
            // binding 9~11: 全局模型矩阵、上帧矩阵、上帧实例数据
            auto globalModelBuffer = vsg::DescriptorBuffer::create(vsg::BufferInfoList{CADMesh::global_model_matrix_buffer_info}, 9, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            auto lastGlobalModelBuffer = vsg::DescriptorBuffer::create(vsg::BufferInfoList{CADMesh::last_global_model_matrix_buffer_info}, 10, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            auto lastProtoBuffer = vsg::DescriptorBuffer::create(vsg::BufferInfoList{proto_data->last_instance_buffer_info}, 11, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{storageBuffer, storageImage, globalModelBuffer, lastGlobalModelBuffer, lastProtoBuffer});
            auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, descriptorSet);
            depth_pyramid_CommandGraph->addChild(bindDescriptorSet);

            // Dispatch: 根据实例数量选择不同的 workgroup 分组方式
            if(proto_data->instance_matrix.size() > 32)
                depth_pyramid_CommandGraph->addChild(vsg::Dispatch::create(proto_data->instance_matrix.size() / 700 + 1, 1, 1));  // 大实例: 每组 700
            else
                depth_pyramid_CommandGraph->addChild(vsg::Dispatch::create(proto_data->instance_matrix.size() / 32 + 1, 1, 1));   // 小实例: 每组 32

            // 收集每个 proto 的 barrier，最后统一插入
            auto indirectBarrier = vsg::BufferMemoryBarrier::create(
                VK_ACCESS_SHADER_WRITE_BIT,
                VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                proto_data->draw_indirect->bufferInfo->buffer,
                0,
                VK_WHOLE_SIZE
            );
            Pass2CullToPass2Barrier->add(indirectBarrier);  // 收集 barrier
        }
        // 最终 barrier: 确保所有 proto 的 compute shader 写入完成后，间接绘制阶段可读取
        depth_pyramid_CommandGraph->addChild(Pass2CullToPass2Barrier);
    }

}
