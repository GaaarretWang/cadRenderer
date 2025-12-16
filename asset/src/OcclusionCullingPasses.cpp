#include "OcclusionCullingPasses.h"

namespace OcclusionCullingPasses{
    int mip_level_count = 10;
    vsg::ref_ptr<vsg::Image> depthPyramidImage;
    vsg::ref_ptr<vsg::Sampler> depth_pyramid_sampler;
    vsg::ref_ptr<vsg::ImageView> depthPyramidImageView;
    vsg::ref_ptr<vsg::ImageInfo> depthPyramidImageInfo;
    vsg::ref_ptr<vsg::ImageInfo> framebuffer_depthImageInfo;

    void initOcclusionCullingPassesImageInfo(VkExtent2D extent, vsg::ref_ptr<vsg::Window> window){
        depthPyramidImage = vsg::Image::create();
        depthPyramidImage->imageType = VK_IMAGE_TYPE_2D;
        depthPyramidImage->format = VK_FORMAT_R32_SFLOAT; // 假设与深度附件兼容
        depthPyramidImage->mipLevels = 10; // 共 7 层
        depthPyramidImage->usage = VK_IMAGE_USAGE_STORAGE_BIT |          // 计算着色器读写
                                    VK_IMAGE_USAGE_SAMPLED_BIT | 
                                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT |     // 可能需要mipmap生成
                                    VK_IMAGE_USAGE_TRANSFER_DST_BIT;      // 可能需要初始化
        depthPyramidImage->initialLayout = VK_IMAGE_LAYOUT_GENERAL;
        depthPyramidImage->extent.width = extent.width;
        depthPyramidImage->extent.height = extent.height;
        depthPyramidImage->extent.depth = 1;

        depth_pyramid_sampler = vsg::Sampler::create();
        depth_pyramid_sampler->minLod = 0;
        depth_pyramid_sampler->maxLod = static_cast<uint32_t>(std::max(0, mip_level_count - 1));
        depth_pyramid_sampler->magFilter = VK_FILTER_NEAREST;  // 放大时使用 Nearest
        depth_pyramid_sampler->minFilter = VK_FILTER_NEAREST;  // 缩小时使用 Nearest
        depth_pyramid_sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST; // Mipmap 使用 Nearest

        depthPyramidImageView = vsg::ImageView::create(depthPyramidImage);
        depthPyramidImageView->subresourceRange.baseMipLevel = 0;
        depthPyramidImageView->subresourceRange.levelCount = mip_level_count;

        depthPyramidImageInfo = vsg::ImageInfo::create(depth_pyramid_sampler, depthPyramidImageView);

        framebuffer_depthImageInfo = vsg::ImageInfo::create(depth_pyramid_sampler, window->getOrCreateDepthImageView());
    }

    CameraPlaneInfo camera_plane_info;
    vsg::ref_ptr<vsg::Array<CameraPlaneInfo>> camera_plane_info_buffer;
    vsg::ref_ptr<vsg::BufferInfo> camera_plane_info_buffer_info;

    vsg::ref_ptr<vsg::mat4Array> camera_matrix = vsg::mat4Array::create(2);
    vsg::ref_ptr<vsg::BufferInfo> camera_matrix_buffer_info;

    void generateCameraData(double fx, double fy, double cx, double cy, double w, double h, double near, double far, vsg::ref_ptr<vsg::Camera> camera){
        camera_plane_info.n[0] = vsg::normalize(vsg::vec4(0, 0, -1, -near));
        camera_plane_info.n[1] = vsg::normalize(vsg::vec4(0, 0, 1, far));
        camera_plane_info.n[2] = vsg::normalize(vsg::vec4(2*fx/w, 0, -2*cx/w, 0));
        camera_plane_info.n[3] = vsg::normalize(vsg::vec4(-2*fx/w, 0, -2+2*cx/w, 0));
        camera_plane_info.n[4] = vsg::normalize(vsg::vec4(0, 2*fy/h, -2+2*cy/h, 0));
        camera_plane_info.n[5] = vsg::normalize(vsg::vec4(0, -2*fy/h, -2*cy/h, 0));

        camera_plane_info_buffer = vsg::Array<CameraPlaneInfo>::create(1);
        camera_plane_info_buffer->set(0, camera_plane_info);

        camera_plane_info_buffer_info = vsg::BufferInfo::create(camera_plane_info_buffer);

        camera_matrix->set(0, vsg::mat4(camera->viewMatrix->transform()));
        camera_matrix->set(1, vsg::mat4(camera->projectionMatrix->transform() * camera->viewMatrix->transform()));
        camera_matrix->properties.dataVariance = vsg::DYNAMIC_DATA;

        camera_matrix_buffer_info = vsg::BufferInfo::create(camera_matrix);
    }

    void buildFirstComputePass(vsg::ref_ptr<vsg::CommandGraph> depth_cull_command_graph1, std::string project_path)
    {
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
        };
        auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);
        auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, vsg::PushConstantRanges{});

        auto ShadowToPass1CullBarrier = vsg::PipelineBarrier::create(
            VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0
        );
        for(auto& proto_data_itr : CADMesh::proto_id_to_data_map){
            ProtoData* proto_data = proto_data_itr.second;
            auto indirectBarrier = vsg::BufferMemoryBarrier::create(
                VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                VK_ACCESS_SHADER_WRITE_BIT,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                proto_data->draw_indirect->bufferInfo->buffer,
                0,
                VK_WHOLE_SIZE
            );
            ShadowToPass1CullBarrier->add(indirectBarrier);
        }
        depth_cull_command_graph1->addChild(ShadowToPass1CullBarrier);

        auto Pass1CullToPass1Barrier = vsg::PipelineBarrier::create(
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
            0
        );
        auto computeShader = vsg::read_cast<vsg::ShaderStage>(project_path + "asset/data/shaders/computevertex.comp", vsg::Options::create());
        auto pipeline = vsg::ComputePipeline::create(pipelineLayout, computeShader);
        auto bindPipeline = vsg::BindComputePipeline::create(pipeline);
        depth_cull_command_graph1->addChild(bindPipeline);

        for(auto& proto_data_itr : CADMesh::proto_id_to_data_map){
            ProtoData* proto_data = proto_data_itr.second;
            auto storageBuffer = vsg::DescriptorBuffer::create(vsg::BufferInfoList{proto_data->draw_indirect->bufferInfo, proto_data->indirect_full_buffer_info,
                                                                                proto_data->input_instance_buffer_info, proto_data->input_highlight_buffer_info, 
                                                                                proto_data->output_instance_buffer_info, camera_plane_info_buffer_info, 
                                                                                proto_data->bounds_buffer_info, camera_matrix_buffer_info}, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{storageBuffer});
            auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, descriptorSet);
            depth_cull_command_graph1->addChild(bindDescriptorSet);
            depth_cull_command_graph1->addChild(vsg::Dispatch::create(proto_data->instance_matrix.size() / 2 / 700 + 1, 1, 1));

            auto indirectBarrier = vsg::BufferMemoryBarrier::create(
                VK_ACCESS_SHADER_WRITE_BIT,
                VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                proto_data->draw_indirect->bufferInfo->buffer,
                0,
                VK_WHOLE_SIZE
            );
            Pass1CullToPass1Barrier->add(indirectBarrier);
        }
        depth_cull_command_graph1->addChild(Pass1CullToPass1Barrier);
    }

    void buildDepthPyramid(vsg::ref_ptr<vsg::CommandGraph> depth_pyramid_CommandGraph, std::string project_path, vsg::ref_ptr<vsg::Window> window, VkExtent2D extent)
    {
        vsg::DescriptorSetLayoutBindings descriptorBindings{
            {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, 
            {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, 
        };
        auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);
        auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, 
                vsg::PushConstantRanges{
                    {VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants)} // projection, view, and model matrices, actual push constant calls automatically provided by the VSG's RecordTraversal
                });
        {
            auto barrier = vsg::ImageMemoryBarrier::create(
                VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
                VK_ACCESS_SHADER_WRITE_BIT,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                depthPyramidImage,
                VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 10, 0, 1}
            );

            auto depthToComputeBarrier = vsg::ImageMemoryBarrier::create(
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,  
                VK_ACCESS_SHADER_READ_BIT,   
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,  
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                window->getOrCreateDepthImage(),
                VkImageSubresourceRange{VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1}
            );

            depth_pyramid_CommandGraph->addChild(vsg::PipelineBarrier::create(
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0, barrier, depthToComputeBarrier
            ));

            auto computeShader = vsg::read_cast<vsg::ShaderStage>(project_path + "asset/data/shaders/computevertex_depthimage.comp", vsg::Options::create());
            auto pipeline = vsg::ComputePipeline::create(pipelineLayout, computeShader);
            auto bindPipeline = vsg::BindComputePipeline::create(pipeline);
            depth_pyramid_CommandGraph->addChild(bindPipeline);
            auto pcData = vsg::Value<ComputePushConstants>::create(ComputePushConstants{extent.width, extent.height});
            depth_pyramid_CommandGraph->addChild(vsg::PushConstants::create(
                VK_SHADER_STAGE_COMPUTE_BIT,
                0,
                pcData
            ));

            auto storageImage = vsg::DescriptorImage::create(vsg::ImageInfoList{depthPyramidImageInfo, framebuffer_depthImageInfo}, 0);
            auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{storageImage});
            auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, descriptorSet);
            depth_pyramid_CommandGraph->addChild(bindDescriptorSet);
            depth_pyramid_CommandGraph->addChild(vsg::Dispatch::create((extent.width + 31) / 32, (extent.height + 31) / 32, 1));

            auto barrier1 = vsg::ImageMemoryBarrier::create(
                VK_ACCESS_SHADER_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                depthPyramidImage,
                VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1} // 前一层级
            );

            depth_pyramid_CommandGraph->addChild(vsg::PipelineBarrier::create(
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0, barrier1
            ));
        }

        for(uint32_t i = 1; i < 10; i ++)
        {
            auto computeShader = vsg::read_cast<vsg::ShaderStage>(project_path + "asset/data/shaders/computevertex_depthpyramid.comp", vsg::Options::create());
            auto pipeline = vsg::ComputePipeline::create(pipelineLayout, computeShader);
            auto bindPipeline = vsg::BindComputePipeline::create(pipeline);
            depth_pyramid_CommandGraph->addChild(bindPipeline);
            auto pcData = vsg::Value<ComputePushConstants>::create(ComputePushConstants{extent.width >> i, extent.height >> i});
            depth_pyramid_CommandGraph->addChild(vsg::PushConstants::create(
                VK_SHADER_STAGE_COMPUTE_BIT,
                0,
                pcData
            ));

            auto i_image_view = vsg::ImageView::create(depthPyramidImage);
            i_image_view->subresourceRange.baseMipLevel = i;
            i_image_view->subresourceRange.levelCount = 1;
            auto i_depthPyramidImageInfo = vsg::ImageInfo::create(depth_pyramid_sampler, i_image_view);
            auto i1_image_view = vsg::ImageView::create(depthPyramidImage);
            i1_image_view->subresourceRange.baseMipLevel = i - 1;
            i1_image_view->subresourceRange.levelCount = 1;
            auto i1_depthPyramidImageInfo = vsg::ImageInfo::create(depth_pyramid_sampler, i1_image_view);
            auto storageImage0 = vsg::DescriptorImage::create(vsg::ImageInfoList{i_depthPyramidImageInfo}, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
            auto storageImage1 = vsg::DescriptorImage::create(vsg::ImageInfoList{i1_depthPyramidImageInfo}, 1, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
            auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{storageImage0, storageImage1});
            auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, descriptorSet);
            depth_pyramid_CommandGraph->addChild(bindDescriptorSet);
            uint32_t mipWidth = std::max(1u, extent.width >> i);
            uint32_t mipHeight = std::max(1u, extent.height >> i);
            depth_pyramid_CommandGraph->addChild(vsg::Dispatch::create(
                (mipWidth + 31) / 32,
                (mipHeight + 31) / 32,
                1
            ));

            auto barrier = vsg::ImageMemoryBarrier::create(
                VK_ACCESS_SHADER_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                depthPyramidImage,
                VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, i, 1, 0, 1} // 前一层级
            );

            depth_pyramid_CommandGraph->addChild(vsg::PipelineBarrier::create(
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0, barrier
            ));
        }
        auto pyramidFinalBarrier = vsg::ImageMemoryBarrier::create(
            VK_ACCESS_SHADER_WRITE_BIT,          // 前序：金字塔生成的写入
            VK_ACCESS_SHADER_READ_BIT,           // 后续：剔除阶段的读取
            VK_IMAGE_LAYOUT_GENERAL,             // 金字塔生成时的布局
            VK_IMAGE_LAYOUT_GENERAL,             // 剔除读取时的布局（保持一致）
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            depthPyramidImage,
            VkImageSubresourceRange{
                VK_IMAGE_ASPECT_COLOR_BIT,       // 关键！depthPyramidImage是R32_SFLOAT（普通颜色格式），不是深度格式，不能用DEPTH_BIT
                0, 10, 0, 1                       // 同步所有7个mip层
            }
        );

        depth_pyramid_CommandGraph->addChild(vsg::PipelineBarrier::create(
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // 前序阶段：金字塔生成的计算阶段
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // 后续阶段：剔除的计算阶段
            0,
            pyramidFinalBarrier
        ));
    }

    void buildSecondComputePass(vsg::ref_ptr<vsg::CommandGraph> depth_pyramid_CommandGraph, std::string project_path, VkExtent2D extent)
        {
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
        };
        auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);
        auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, 
                vsg::PushConstantRanges{
                    {VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants)} // projection, view, and model matrices, actual push constant calls automatically provided by the VSG's RecordTraversal
                });

        auto Pass2CullToPass2Barrier = vsg::PipelineBarrier::create(
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
            0
        );
        auto computeShader = vsg::read_cast<vsg::ShaderStage>(project_path + "asset/data/shaders/computevertex1.comp", vsg::Options::create());
        auto pipeline = vsg::ComputePipeline::create(pipelineLayout, computeShader);
        auto bindPipeline = vsg::BindComputePipeline::create(pipeline);
        auto computeShader_seat = vsg::read_cast<vsg::ShaderStage>(project_path + "asset/data/shaders/computevertex1_seat.comp", vsg::Options::create());
        auto pipeline_seat = vsg::ComputePipeline::create(pipelineLayout, computeShader_seat);
        auto bindPipeline_seat = vsg::BindComputePipeline::create(pipeline_seat);
        depth_pyramid_CommandGraph->addChild(bindPipeline);
        auto pcData1 = vsg::Value<ComputePushConstants>::create(ComputePushConstants{extent.width, extent.height});
        depth_pyramid_CommandGraph->addChild(vsg::PushConstants::create(
            VK_SHADER_STAGE_COMPUTE_BIT,
            0,
            pcData1
        ));

        auto pre_pipeline = bindPipeline;
        for(auto& proto_data_itr : CADMesh::proto_id_to_data_map){
            ProtoData* proto_data = proto_data_itr.second;
            if(proto_data->instance_matrix.size() / 2 > 32 && pre_pipeline == bindPipeline){
                depth_pyramid_CommandGraph->addChild(bindPipeline_seat);
                pre_pipeline = bindPipeline_seat;
            }
            else if(proto_data->instance_matrix.size() / 2 <= 32 && pre_pipeline == bindPipeline_seat){
                depth_pyramid_CommandGraph->addChild(bindPipeline);
                pre_pipeline = bindPipeline;
            }
            auto storageBuffer = vsg::DescriptorBuffer::create(vsg::BufferInfoList{proto_data->draw_indirect->bufferInfo, proto_data->indirect_full_buffer_info,
                                                                                proto_data->input_instance_buffer_info, proto_data->input_highlight_buffer_info, 
                                                                                proto_data->output_instance_buffer_info, camera_plane_info_buffer_info, 
                                                                                proto_data->bounds_buffer_info, camera_matrix_buffer_info}, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            auto storageImage = vsg::DescriptorImage::create(vsg::ImageInfoList{depthPyramidImageInfo}, 8);
            auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{storageBuffer, storageImage});
            auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, descriptorSet);
            depth_pyramid_CommandGraph->addChild(bindDescriptorSet);
            if(proto_data->instance_matrix.size() / 2 > 32)
                depth_pyramid_CommandGraph->addChild(vsg::Dispatch::create(proto_data->instance_matrix.size() / 2 / 700 + 1, 1, 1));
            else
                depth_pyramid_CommandGraph->addChild(vsg::Dispatch::create(proto_data->instance_matrix.size() / 2 / 32 + 1, 1, 1));
            auto indirectBarrier = vsg::BufferMemoryBarrier::create(
                VK_ACCESS_SHADER_WRITE_BIT,
                VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                proto_data->draw_indirect->bufferInfo->buffer,
                0,
                VK_WHOLE_SIZE
            );
            Pass2CullToPass2Barrier->add(indirectBarrier);
        }
        depth_pyramid_CommandGraph->addChild(Pass2CullToPass2Barrier);
    }

}
