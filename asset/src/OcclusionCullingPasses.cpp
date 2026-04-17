#include "OcclusionCullingPasses.h"

namespace OcclusionCullingPasses{
    int mip_level_count = 10;
    vsg::ref_ptr<vsg::Image> depthPyramidImage;
    vsg::ref_ptr<vsg::Sampler> depth_pyramid_sampler;
    vsg::ref_ptr<vsg::ImageView> depthPyramidImageView;
    vsg::ref_ptr<vsg::ImageInfo> depthPyramidImageInfo;
    vsg::ref_ptr<vsg::ImageInfo> framebuffer_depthImageInfo;

    void initOcclusionCullingPassesImageInfo(VkExtent2D extent, vsg::ref_ptr<OffscreenRenderTarget> offscreenTarget){
        depthPyramidImage = vsg::Image::create();
        depthPyramidImage->imageType = VK_IMAGE_TYPE_2D;
        depthPyramidImage->format = VK_FORMAT_R32_SFLOAT; // Treat it as compatible with the depth data we need.
        depthPyramidImage->mipLevels = 10; // Total mip count.
        depthPyramidImage->arrayLayers = 1; // Single array layer.
        depthPyramidImage->usage = VK_IMAGE_USAGE_STORAGE_BIT |          // Read and write from compute shaders.
                                    VK_IMAGE_USAGE_SAMPLED_BIT | 
                                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT |     // May be needed for mip generation.
                                    VK_IMAGE_USAGE_TRANSFER_DST_BIT;      // May be needed for initialization.
        depthPyramidImage->initialLayout = VK_IMAGE_LAYOUT_GENERAL;
        depthPyramidImage->extent.width = extent.width;
        depthPyramidImage->extent.height = extent.height;
        depthPyramidImage->extent.depth = 1;

        depth_pyramid_sampler = vsg::Sampler::create();
        depth_pyramid_sampler->minLod = 0;
        depth_pyramid_sampler->maxLod = static_cast<uint32_t>(std::max(0, mip_level_count - 1));
        depth_pyramid_sampler->magFilter = VK_FILTER_NEAREST;  // Use nearest filtering when magnifying.
        depth_pyramid_sampler->minFilter = VK_FILTER_NEAREST;  // Use nearest filtering when minifying.
        depth_pyramid_sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST; // Use nearest filtering between mip levels.

        depthPyramidImageView = vsg::ImageView::create(depthPyramidImage);
        depthPyramidImageView->subresourceRange.baseMipLevel = 0;
        depthPyramidImageView->subresourceRange.levelCount = mip_level_count;

        depthPyramidImageInfo = vsg::ImageInfo::create(depth_pyramid_sampler, depthPyramidImageView, VK_IMAGE_LAYOUT_GENERAL);

        framebuffer_depthImageInfo = vsg::ImageInfo::create(depth_pyramid_sampler, offscreenTarget->depthImageView);
    }

    CameraPlaneInfo camera_plane_info;
    vsg::ref_ptr<vsg::Array<CameraPlaneInfo>> camera_plane_info_buffer;
    vsg::ref_ptr<vsg::BufferInfo> camera_plane_info_buffer_info;

    vsg::ref_ptr<vsg::mat4Array> camera_matrix = vsg::mat4Array::create(2);
    vsg::ref_ptr<vsg::BufferInfo> camera_matrix_buffer_info;

    void generateCameraData(double fx, double fy, double cx, double cy, double w, double h, double near_plane, double far_plane, vsg::ref_ptr<vsg::Camera> camera){
        camera_plane_info.n[0] = vsg::normalize(vsg::vec4(0, 0, -1, -near_plane));
        camera_plane_info.n[1] = vsg::normalize(vsg::vec4(0, 0, 1, far_plane));
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

    void buildFirstComputePass(vsg::ref_ptr<vsg::CommandGraph> depth_cull_command_graph1, vsg::ref_ptr<vsg::Options> options)
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
            {9, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {10, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {11, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
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
            // if (proto_data->is_transparent) continue;
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
        auto shaderPath = vsg::findFile("shaders/computevertex.comp", options->paths);
        auto computeShader = vsg::read_cast<vsg::ShaderStage>(shaderPath, options);
        auto pipeline = vsg::ComputePipeline::create(pipelineLayout, computeShader);
        auto bindPipeline = vsg::BindComputePipeline::create(pipeline);
        depth_cull_command_graph1->addChild(bindPipeline);

        for(auto& proto_data_itr : CADMesh::proto_id_to_data_map){
            ProtoData* proto_data = proto_data_itr.second;
            // if (proto_data->is_transparent) continue;
            auto storageBuffer = vsg::DescriptorBuffer::create(vsg::BufferInfoList{proto_data->draw_indirect->bufferInfo, proto_data->indirect_full_buffer_info,
                                                                                proto_data->input_instance_buffer_info, proto_data->input_highlight_buffer_info,
                                                                                proto_data->output_instance_buffer_info, camera_plane_info_buffer_info,
                                                                                proto_data->bounds_buffer_info, camera_matrix_buffer_info}, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            auto globalModelBuffer = vsg::DescriptorBuffer::create(vsg::BufferInfoList{CADMesh::global_model_matrix_buffer_info}, 9, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            auto lastGlobalModelBuffer = vsg::DescriptorBuffer::create(vsg::BufferInfoList{CADMesh::last_global_model_matrix_buffer_info}, 10, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            auto lastProtoBuffer = vsg::DescriptorBuffer::create(vsg::BufferInfoList{proto_data->last_instance_buffer_info}, 11, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{storageBuffer, globalModelBuffer, lastGlobalModelBuffer, lastProtoBuffer});
            auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, descriptorSet);
            depth_cull_command_graph1->addChild(bindDescriptorSet);
            depth_cull_command_graph1->addChild(vsg::Dispatch::create(proto_data->instance_matrix.size() / 700 + 1, 1, 1));

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

    void buildDepthPyramid(vsg::ref_ptr<vsg::CommandGraph> depth_pyramid_CommandGraph, vsg::ref_ptr<vsg::Options> options, VkExtent2D extent, vsg::ref_ptr<OffscreenRenderTarget> offscreenTarget)
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
                offscreenTarget->depthImage,
                VkImageSubresourceRange{VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1}
            );

            depth_pyramid_CommandGraph->addChild(vsg::PipelineBarrier::create(
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0, barrier, depthToComputeBarrier
            ));

            auto shaderPath = vsg::findFile("shaders/computevertex_depthimage.comp", options->paths);
            auto computeShader = vsg::read_cast<vsg::ShaderStage>(shaderPath, options);
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
                VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1} // Previous mip level.
            );

            depth_pyramid_CommandGraph->addChild(vsg::PipelineBarrier::create(
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0, barrier1
            ));
        }

        for(uint32_t i = 1; i < 10; i ++)
        {
            auto shaderPath = vsg::findFile("shaders/computevertex_depthpyramid.comp", options->paths);
            auto computeShader = vsg::read_cast<vsg::ShaderStage>(shaderPath, options);
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
            auto i_depthPyramidImageInfo = vsg::ImageInfo::create(depth_pyramid_sampler, i_image_view, VK_IMAGE_LAYOUT_GENERAL);
            auto i1_image_view = vsg::ImageView::create(depthPyramidImage);
            i1_image_view->subresourceRange.baseMipLevel = i - 1;
            i1_image_view->subresourceRange.levelCount = 1;
            auto i1_depthPyramidImageInfo = vsg::ImageInfo::create(depth_pyramid_sampler, i1_image_view, VK_IMAGE_LAYOUT_GENERAL);
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
                VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, i, 1, 0, 1} // Previous mip level.
            );

            depth_pyramid_CommandGraph->addChild(vsg::PipelineBarrier::create(
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0, barrier
            ));
        }
        auto pyramidFinalBarrier = vsg::ImageMemoryBarrier::create(
            VK_ACCESS_SHADER_WRITE_BIT,          // Previous step: depth-pyramid writes.
            VK_ACCESS_SHADER_READ_BIT,           // Next step: culling reads.
            VK_IMAGE_LAYOUT_GENERAL,             // Layout during depth-pyramid generation.
            VK_IMAGE_LAYOUT_GENERAL,             // Layout used while culling reads it.
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            depthPyramidImage,
            VkImageSubresourceRange{
                VK_IMAGE_ASPECT_COLOR_BIT,       // Important: depthPyramidImage is R32_SFLOAT, not a depth format.
                0, 10, 0, 1                       // Synchronize all mip levels.
            }
        );

        depth_pyramid_CommandGraph->addChild(vsg::PipelineBarrier::create(
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // Previous stage: depth-pyramid generation.
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // Next stage: culling compute pass.
            0,
            pyramidFinalBarrier
        ));
    }

    void buildSecondComputePass(vsg::ref_ptr<vsg::CommandGraph> depth_pyramid_CommandGraph, vsg::ref_ptr<vsg::Options> options, VkExtent2D extent)
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
            {9, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {10, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {11, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
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
        auto shaderPath = vsg::findFile("shaders/computevertex1.comp", options->paths);
        auto computeShader = vsg::read_cast<vsg::ShaderStage>(shaderPath, options);
        auto pipeline = vsg::ComputePipeline::create(pipelineLayout, computeShader);
        auto bindPipeline = vsg::BindComputePipeline::create(pipeline);
        auto shaderPath_seat = vsg::findFile("shaders/computevertex1_seat.comp", options->paths);
        auto computeShader_seat = vsg::read_cast<vsg::ShaderStage>(shaderPath_seat, options);
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
            // if (proto_data->is_transparent) continue;
            if(proto_data->instance_matrix.size() > 32 && pre_pipeline == bindPipeline){
                depth_pyramid_CommandGraph->addChild(bindPipeline_seat);
                pre_pipeline = bindPipeline_seat;
            }
            else if(proto_data->instance_matrix.size() <= 32 && pre_pipeline == bindPipeline_seat){
                depth_pyramid_CommandGraph->addChild(bindPipeline);
                pre_pipeline = bindPipeline;
            }
            auto storageBuffer = vsg::DescriptorBuffer::create(vsg::BufferInfoList{proto_data->draw_indirect->bufferInfo, proto_data->indirect_full_buffer_info,
                                                                                proto_data->input_instance_buffer_info, proto_data->input_highlight_buffer_info,
                                                                                proto_data->output_instance_buffer_info, camera_plane_info_buffer_info,
                                                                                proto_data->bounds_buffer_info, camera_matrix_buffer_info}, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            auto storageImage = vsg::DescriptorImage::create(vsg::ImageInfoList{depthPyramidImageInfo}, 8);
            auto globalModelBuffer = vsg::DescriptorBuffer::create(vsg::BufferInfoList{CADMesh::global_model_matrix_buffer_info}, 9, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            auto lastGlobalModelBuffer = vsg::DescriptorBuffer::create(vsg::BufferInfoList{CADMesh::last_global_model_matrix_buffer_info}, 10, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            auto lastProtoBuffer = vsg::DescriptorBuffer::create(vsg::BufferInfoList{proto_data->last_instance_buffer_info}, 11, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{storageBuffer, storageImage, globalModelBuffer, lastGlobalModelBuffer, lastProtoBuffer});
            auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, descriptorSet);
            depth_pyramid_CommandGraph->addChild(bindDescriptorSet);
            if(proto_data->instance_matrix.size() > 32)
                depth_pyramid_CommandGraph->addChild(vsg::Dispatch::create(proto_data->instance_matrix.size() / 700 + 1, 1, 1));
            else
                depth_pyramid_CommandGraph->addChild(vsg::Dispatch::create(proto_data->instance_matrix.size() / 32 + 1, 1, 1));
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
