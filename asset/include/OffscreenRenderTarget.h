#pragma once

#include "vsg/all.h"

class OffscreenRenderTarget : public vsg::Inherit<vsg::Object, OffscreenRenderTarget>
{
public:
    // Create all MRT images and depth image
    void init(vsg::ref_ptr<vsg::Device> device, VkExtent2D extent, VkSampleCountFlagBits samples, VkFormat depthFormat, VkImageUsageFlags depthImageUsage);

    // Create custom render pass (like createMRTRenderPass but with offscreen layout for attachment[0])
    void buildRenderPass(vsg::ref_ptr<vsg::Device> device, VkFormat imageFormat, VkFormat depthFormat, bool requiresDepthRead);

    // Create framebuffer using the render pass and all image views
    void buildFramebuffer(VkExtent2D extent);

    VkExtent2D getExtent() const { return _extent; }
    bool isMultisampled() const { return _samples != VK_SAMPLE_COUNT_1_BIT; }

    // GBuffer images
    vsg::ref_ptr<vsg::Image> gbufferImage0;
    vsg::ref_ptr<vsg::ImageView> gbufferImageView0;

    vsg::ref_ptr<vsg::Image> gbufferImage1;
    vsg::ref_ptr<vsg::ImageView> gbufferImageView1;

    vsg::ref_ptr<vsg::Image> gbufferImage2;
    vsg::ref_ptr<vsg::ImageView> gbufferImageView2;

    // Shadow write
    vsg::ref_ptr<vsg::Image> shadowWriteImage;
    vsg::ref_ptr<vsg::ImageView> shadowWriteImageView;

    // Shadow sample
    vsg::ref_ptr<vsg::Image> shadowSampleImage;
    vsg::ref_ptr<vsg::ImageView> shadowSampleImageView;

    // Depth
    vsg::ref_ptr<vsg::Image> depthImage;
    vsg::ref_ptr<vsg::ImageView> depthImageView;

    // Multisample depth (only when multisampling + requiresDepthRead)
    vsg::ref_ptr<vsg::Image> multisampleDepthImage;
    vsg::ref_ptr<vsg::ImageView> multisampleDepthImageView;

    // Multisample color (only when multisampling)
    vsg::ref_ptr<vsg::Image> multisampleImage;
    vsg::ref_ptr<vsg::ImageView> multisampleImageView;

    // Offscreen color attachment (for blit to window)
    vsg::ref_ptr<vsg::Image> colorImage;
    vsg::ref_ptr<vsg::ImageView> colorImageView;

    // Render pass
    vsg::ref_ptr<vsg::RenderPass> renderPass;

    // Framebuffer
    vsg::ref_ptr<vsg::Framebuffer> framebuffer;

private:
    VkExtent2D _extent;
    VkSampleCountFlagBits _samples;
};

class ColorRenderTarget : public vsg::Inherit<vsg::Object, ColorRenderTarget>
{
public:
    void init(vsg::ref_ptr<vsg::Device> device,
              VkExtent2D extent,
              VkFormat imageFormat,
              VkImageUsageFlags extraUsage = 0,
              VkImageLayout finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
              VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);

    VkExtent2D getExtent() const { return _extent; }
    bool isMultisampled() const { return _samples != VK_SAMPLE_COUNT_1_BIT; }
    uint32_t clearValueCount() const { return isMultisampled() ? 2u : 1u; }

    vsg::ref_ptr<vsg::Image> multisampleImage;
    vsg::ref_ptr<vsg::ImageView> multisampleImageView;

    vsg::ref_ptr<vsg::Image> colorImage;
    vsg::ref_ptr<vsg::ImageView> colorImageView;
    vsg::ref_ptr<vsg::RenderPass> renderPass;
    vsg::ref_ptr<vsg::Framebuffer> framebuffer;

private:
    VkExtent2D _extent{};
    VkSampleCountFlagBits _samples = VK_SAMPLE_COUNT_1_BIT;
};
