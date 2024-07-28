#pragma once

/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/state/ImageView.h>
#include <vsg/vk/Fence.h>
#include <vsg/vk/Surface.h>

namespace vsg
{
    /// Swapchain preferences passed via WindowTraits::swapchainPreferences to guide swapchain creation associated with Window creation.
    struct OffScreenSwapchainPreferences
    {
        uint32_t imageCount = 3; // default to triple buffering
        VkSurfaceFormatKHR surfaceFormat = {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
        VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
        VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        VkSurfaceTransformFlagBitsKHR preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    };

    /// Swapchain encapsulates vkSwapchainKHR
    class VSG_DECLSPEC OffScreenSwapchain : public Inherit<Object, OffScreenSwapchain>
    {
    public:
        OffScreenSwapchain(PhysicalDevice* physicalDevice, Device* device, Surface* surface, uint32_t width, uint32_t height, OffScreenSwapchainPreferences& preferences, ref_ptr<OffScreenSwapchain> oldSwapchain = {});

        operator VkSwapchainKHR() const { return _swapchain; }
        VkSwapchainKHR vk() const { return _swapchain; }

        VkFormat getImageFormat() const { return _format; }

        const VkExtent2D& getExtent() const { return _extent; }

        ImageViews& getImageViews() { return _imageViews; }
        const ImageViews& getImageViews() const { return _imageViews; }

        /// call vkAcquireNextImageKHR
        VkResult acquireNextImage(uint64_t timeout, ref_ptr<Semaphore> semaphore, ref_ptr<Fence> fence, uint32_t& imageIndex);

    protected:
        virtual ~OffScreenSwapchain();

        vsg::ref_ptr<Device> _device;
        vsg::ref_ptr<Surface> _surface;
        VkSwapchainKHR _swapchain;
        VkFormat _format;
        VkExtent2D _extent;
        ImageViews _imageViews;
        std::vector<VkImage> _images;
    };
    VSG_type_name(vsg::OffScreenSwapchain);

} // namespace vsg
