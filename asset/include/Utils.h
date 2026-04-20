#ifndef UTILS_H
#define UTILS_H

#include "vsg/all.h"
#include "OffscreenRenderTarget.h"

namespace Utils{
    inline vsg::ref_ptr<vsg::Sampler> createLinearSampler()
    {
        static vsg::ref_ptr<vsg::Sampler> sampler = []() {
            auto s = vsg::Sampler::create();
            
            s->magFilter = VK_FILTER_LINEAR;
            s->minFilter = VK_FILTER_LINEAR;
            s->mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            
            s->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            s->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            s->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            
            return s;
        }();

        return sampler;
    }

    inline vsg::ref_ptr<vsg::Sampler> createNearestSampler()
    {
        static vsg::ref_ptr<vsg::Sampler> sampler = []() {
            auto s = vsg::Sampler::create();
            
            s->magFilter = VK_FILTER_NEAREST;
            s->minFilter = VK_FILTER_NEAREST;
            s->mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            
            s->addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            s->addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            s->addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            
            return s;
        }();

        return sampler;
    }

    inline vsg::ref_ptr<vsg::Sampler> createNearestClampSampler()
    {
        static vsg::ref_ptr<vsg::Sampler> sampler = []() {
            auto s = vsg::Sampler::create();
            
            s->magFilter = VK_FILTER_NEAREST;
            s->minFilter = VK_FILTER_NEAREST;
            s->mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            
            s->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            s->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            s->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            
            return s;
        }();

        return sampler;
    }

    inline void BuildClearCommandGraph(vsg::ref_ptr<vsg::CommandGraph> clear_image_commandgraph, VkExtent2D extent, vsg::ref_ptr<OffscreenRenderTarget> offscreenTarget, VkSampleCountFlagBits msaaSamples){
        vsg::ref_ptr<vsg::ClearDepthStencilImage> clearDepth = vsg::ClearDepthStencilImage::create();
        vsg::ref_ptr<vsg::ClearDepthStencilImage> clearDepth1 = vsg::ClearDepthStencilImage::create();

        auto addBarrier = [&](VkPipelineStageFlags srcStage,
                              VkPipelineStageFlags dstStage,
                              vsg::ref_ptr<vsg::ImageMemoryBarrier> imageBarrier) {
            clear_image_commandgraph->addChild(vsg::PipelineBarrier::create(
                srcStage,
                dstStage,
                0,
                imageBarrier));
        };

        clearDepth->depthStencil = {0.0f, 0};
        VkImageSubresourceRange range{};
        range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        range.baseMipLevel = 0;
        range.levelCount = 1;
        range.baseArrayLayer = 0;
        range.layerCount = 1;
        clearDepth->ranges = {range};
        clearDepth->imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;


        clearDepth1->depthStencil = {0.0f, 0};
        clearDepth1->ranges = {range};
        clearDepth1->imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        if(msaaSamples != VK_SAMPLE_COUNT_1_BIT)
            clearDepth->image = offscreenTarget->multisampleDepthImage;
        clearDepth1->image = offscreenTarget->depthImage;

        if (msaaSamples != VK_SAMPLE_COUNT_1_BIT)
        {
            addBarrier(
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                vsg::ImageMemoryBarrier::create(
                    0,
                    VK_ACCESS_TRANSFER_WRITE_BIT,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_QUEUE_FAMILY_IGNORED,
                    VK_QUEUE_FAMILY_IGNORED,
                    offscreenTarget->multisampleDepthImage,
                    range));
        }
        addBarrier(
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            vsg::ImageMemoryBarrier::create(
                0,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                offscreenTarget->depthImage,
                range));

        if(msaaSamples != VK_SAMPLE_COUNT_1_BIT)
            clear_image_commandgraph->addChild(clearDepth);
        clear_image_commandgraph->addChild(clearDepth1);


        VkImageSubresourceRange range0{};
        range0.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range0.baseMipLevel = 0;
        range0.levelCount = 1;
        range0.baseArrayLayer = 0;
        range0.layerCount = 1;

        auto clearColor0 = vsg::ClearColorImage::create();
        clearColor0->image = offscreenTarget->gbufferImage0;
        clearColor0->imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        clearColor0->color = {{0.0f, 0.0f, 0.0f, 0.0f}};
        clearColor0->ranges = {range0};

        auto clearColor1 = vsg::ClearColorImage::create();
        clearColor1->image = offscreenTarget->gbufferImage1;
        clearColor1->imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        clearColor1->color = {{0.0f, 0.0f, 0.0f, 0.0f}};
        clearColor1->ranges = {range0};

        auto clearColor2 = vsg::ClearColorImage::create();
        clearColor2->image = offscreenTarget->gbufferImage2;
        clearColor2->imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        clearColor2->color = {{0.0f, 0.0f, 0.0f, 0.0f}};
        clearColor2->ranges = {range0};

        auto clearColor3 = vsg::ClearColorImage::create();
        clearColor3->image = offscreenTarget->materialImage;
        clearColor3->imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        clearColor3->color = {{0.0f, 0.0f, 0.0f, 0.0f}};
        clearColor3->ranges = {range0};

        auto clearColor4 = vsg::ClearColorImage::create();
        clearColor4->image = offscreenTarget->shadowWriteImage;
        clearColor4->imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        clearColor4->color = {{0.0f, 0.0f, 0.0f, 0.0f}};
        clearColor4->ranges = {range0};

        addBarrier(
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            vsg::ImageMemoryBarrier::create(
                0,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                offscreenTarget->gbufferImage0,
                range0));
        addBarrier(
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            vsg::ImageMemoryBarrier::create(
                0,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                offscreenTarget->gbufferImage1,
                range0));
        addBarrier(
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            vsg::ImageMemoryBarrier::create(
                0,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                offscreenTarget->gbufferImage2,
                range0));
        addBarrier(
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            vsg::ImageMemoryBarrier::create(
                0,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                offscreenTarget->materialImage,
                range0));
        addBarrier(
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            vsg::ImageMemoryBarrier::create(
                0,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                offscreenTarget->shadowWriteImage,
                range0));

        clear_image_commandgraph->addChild(clearColor0);
        clear_image_commandgraph->addChild(clearColor1);
        clear_image_commandgraph->addChild(clearColor2);
        clear_image_commandgraph->addChild(clearColor3);
        clear_image_commandgraph->addChild(clearColor4);

        if (msaaSamples != VK_SAMPLE_COUNT_1_BIT)
        {
            addBarrier(
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                vsg::ImageMemoryBarrier::create(
                    VK_ACCESS_TRANSFER_WRITE_BIT,
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                    VK_QUEUE_FAMILY_IGNORED,
                    VK_QUEUE_FAMILY_IGNORED,
                    offscreenTarget->multisampleDepthImage,
                    range));
        }
        addBarrier(
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            vsg::ImageMemoryBarrier::create(
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                offscreenTarget->depthImage,
                range));
        addBarrier(
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            vsg::ImageMemoryBarrier::create(
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                offscreenTarget->gbufferImage0,
                range0));
        addBarrier(
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            vsg::ImageMemoryBarrier::create(
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                offscreenTarget->gbufferImage1,
                range0));
        addBarrier(
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            vsg::ImageMemoryBarrier::create(
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                offscreenTarget->gbufferImage2,
                range0));
        addBarrier(
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            vsg::ImageMemoryBarrier::create(
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                offscreenTarget->materialImage,
                range0));
        addBarrier(
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            vsg::ImageMemoryBarrier::create(
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                offscreenTarget->shadowWriteImage,
                range0));

        // auto clearCmd = vsg::ClearAttachments::create();
        // VkClearRect clearRect{};
        // clearRect.rect = {




        // };




        // {
        //     VkClearAttachment attachment{};



        //     clearCmd->attachments.push_back(attachment);
        //     clearCmd->rects.push_back(clearRect);
        // }

        // clear_image_commandgraph->addChild(clearCmd);


    }
}

#endif // UTILS_H
