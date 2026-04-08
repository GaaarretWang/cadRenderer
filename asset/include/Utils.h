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
        

        clearDepth->depthStencil = {0.0f, 0};
        VkImageSubresourceRange range{};
        range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        range.baseMipLevel = 0;
        range.levelCount = 1;
        range.baseArrayLayer = 0;
        range.layerCount = 1;
        clearDepth->ranges = {range};


        clearDepth1->depthStencil = {0.0f, 0};
        clearDepth1->ranges = {range};
        if(msaaSamples != VK_SAMPLE_COUNT_1_BIT)
            clearDepth->image = offscreenTarget->multisampleDepthImage;
        clearDepth1->image = offscreenTarget->depthImage;

        if(msaaSamples != VK_SAMPLE_COUNT_1_BIT)
            clear_image_commandgraph->addChild(clearDepth);
        clear_image_commandgraph->addChild(clearDepth1);


        VkImageSubresourceRange range0{};

        range0.baseMipLevel = 0;
        range0.levelCount = 1;
        range0.baseArrayLayer = 0;
        range0.layerCount = 1;

        auto clearColor0 = vsg::ClearColorImage::create();
        clearColor0->image = offscreenTarget->gbufferImage0;


        clearColor0->ranges = {range0};

        auto clearColor1 = vsg::ClearColorImage::create();
        clearColor1->image = offscreenTarget->gbufferImage1;


        clearColor1->ranges = {range0};

        auto clearColor2 = vsg::ClearColorImage::create();
        clearColor2->image = offscreenTarget->gbufferImage2;


        clearColor2->ranges = {range0};

        auto clearColor3 = vsg::ClearColorImage::create();
        clearColor3->image = offscreenTarget->shadowWriteImage;


        clearColor3->ranges = {range0};

        clear_image_commandgraph->addChild(clearColor0);
        clear_image_commandgraph->addChild(clearColor1);
        clear_image_commandgraph->addChild(clearColor2);
        clear_image_commandgraph->addChild(clearColor3);

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
