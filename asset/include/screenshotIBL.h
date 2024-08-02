#pragma once
#include <vsg/all.h>

#ifndef SCREENSHOTIBL_H
#define SCREENSHOTIBL_H

namespace IBLScreenshot{
    class ScreenshotHandler : public vsg::Inherit<vsg::Visitor, ScreenshotHandler>
    {
    public:
        bool do_image_capture = 1; //控制是否进行图像捕捉
        bool do_depth_capture = 1; //控制是否进行深度捕捉
        vsg::ref_ptr<vsg::Event> event;
        bool eventDebugTest = false; //用于调试目的


        //构造函数
        ScreenshotHandler(vsg::ref_ptr<vsg::Event> in_event) :
            event(in_event)
        {
        }

        void printInfo(vsg::ref_ptr<vsg::Window> window)
        {//打印窗口和设备的信息;接受一个 vsg::Window 对象作为参数，并输出与窗口相关的信息，如设备、交换链、图像视图等
            auto device = window->getDevice();
            auto physicalDevice = window->getPhysicalDevice();
            auto swapchain = window->getSwapchain();
            std::cout << "\nNeed to take screenshot " << window << std::endl;
            std::cout << "    device = " << device << std::endl;
            std::cout << "    physicalDevice = " << physicalDevice << std::endl;
            std::cout << "    swapchain = " << swapchain << std::endl;
            std::cout << "        swapchain->getImageFormat() = " << swapchain->getImageFormat() << std::endl;
            std::cout << "        swapchain->getExtent() = " << swapchain->getExtent().width << ", " << swapchain->getExtent().height << std::endl;

            for (auto& imageView : swapchain->getImageViews())
            {
                std::cout << "        imageview = " << imageView << std::endl;
            }

            std::cout << "    numFrames() = " << window->numFrames() << std::endl;
            for (size_t i = 0; i < window->numFrames(); ++i)
            {
                std::cout << "        imageview[" << i << "] = " << window->imageView(i) << std::endl;
                std::cout << "        framebuffer[" << i << "] = " << window->framebuffer(i) << std::endl;
            }

            std::cout << "    surfaceFormat() = " << window->surfaceFormat().format << ", " << window->surfaceFormat().colorSpace << std::endl;
            std::cout << "    depthFormat() = " << window->depthFormat() << std::endl;
        }

        vsg::ref_ptr<vsg::Image> screenshot_image(vsg::ref_ptr<vsg::Window> window)
        {
            do_image_capture = 1; //需要进行图像截取

            if (eventDebugTest && event && event->status() == VK_EVENT_RESET)
            { //检查是否需要等待一个事件的信号。如果满足条件，将会进入循环等待事件信号
                std::cout << "event->status() == VK_EVENT_RESET" << std::endl;
                // manually wait for the event to be signaled
                while (event->status() == VK_EVENT_RESET)
                {
                    std::cout << "w";
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                std::cout << std::endl;
            }

            auto width = window->extent2D().width;
            auto height = window->extent2D().height; //获取窗口的宽度和高度

            auto swapchain = window->getSwapchain(); //获取与窗口相关的设备、物理设备和交换链
            VkFormat sourceImageFormat = swapchain->getImageFormat();

            // get the colour buffer image of the previous rendered frame as the current frame hasn't been rendered yet.  The 1 in window->imageIndex(1) means image from 1 frame ago.
            // 下标要为0
            auto sourceImage = window->imageView(window->imageIndex(0))->image; //获取之前渲染帧的颜色缓冲图像作为当前帧的来源图像 window->imageIndex(1)表示获取之前一帧的图像
            
            //vsg::ref_ptr<vsg::Image> storageImage = vsg::Image::create();
            //storageImage->imageType = VK_IMAGE_TYPE_2D;
            //storageImage->format = VK_FORMAT_B8G8R8A8_UNORM; //VK_FORMAT_R8G8B8A8_UNORM;
            //storageImage->extent.width = width;
            //storageImage->extent.height = height;
            //storageImage->extent.depth = 1;
            //storageImage->mipLevels = 1;
            //storageImage->arrayLayers = 1;
            //storageImage->samples = VK_SAMPLE_COUNT_1_BIT;
            //storageImage->tiling = VK_IMAGE_TILING_OPTIMAL;
            //storageImage->usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            //storageImage->initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            //storageImage->flags = 0;
            //storageImage->sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            //auto context = vsg::Context::create(window->getOrCreateDevice());
            //imageInfo = vsg::ImageInfo::create(vsg::ref_ptr<vsg::Sampler>{}, createImageView(*context, storageImage, VK_IMAGE_ASPECT_COLOR_BIT), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            
            return sourceImage;
        }

        vsg::ref_ptr<vsg::Image> screenshot_depth(vsg::ref_ptr<vsg::Window> window)
        {
            do_depth_capture = 1; //设置截取标志

            //则进入循环，直到事件被触发（状态变为VK_EVENT_SET）为止
            if (eventDebugTest && event && event->status() == VK_EVENT_RESET)
            {
                std::cout << "event->status() == VK_EVENT_RESET" << std::endl;
                // manually wait for the event to be signaled
                while (event->status() == VK_EVENT_RESET)
                {
                    std::cout << "w";
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                std::cout << std::endl;
            }

            auto width = window->extent2D().width;
            auto height = window->extent2D().height; //获取窗口大小

            auto device = window->getDevice();
            auto physicalDevice = window->getPhysicalDevice(); //获取设备和物理设备

            //获取源图像和图像格式 sourceImage 是当前窗口渲染绘制的画面的深度图像
            vsg::ref_ptr<vsg::Image> sourceImage(window->getDepthImage());
            //sourceImage表示源图像，sourceImageFormat表示源图像的格式 targetImageFormat表示目标图像的格式
            VkFormat sourceImageFormat = window->depthFormat();
            return sourceImage;
        }
    };
}

#endif