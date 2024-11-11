#pragma  once
#include <iostream>
#include <screenshot.h>
#include <vsg/all.h>
#include "convertPng.h"
#include "ConfigShader.h"
#include "ModelInstance.h"

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

using namespace std;

class vsgRendererClient
{
    vsg::ref_ptr<vsg::Viewer> decode_viewer = vsg::Viewer::create();
    vsg::ref_ptr<ScreenshotHandler> decode_screenshotHandler;
    vsg::ref_ptr<vsg::Window> decode_window;

    int render_width;
    int render_height;

    vsg::ref_ptr<vsg::WindowTraits> createWindowTraits(string windowTitle, int num,  vsg::ref_ptr<vsg::Options> options)
    {
        auto windowTraits = vsg::WindowTraits::create();
        windowTraits->windowTitle = windowTitle;
        windowTraits->width = render_width;
        windowTraits->height = render_height;
        windowTraits->x = render_width * (num % 2);
        windowTraits->y = render_height * (num / 2);
        // enable transfer from the colour and depth buffer images
        windowTraits->swapchainPreferences.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        windowTraits->depthImageUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        windowTraits->depthFormat = VK_FORMAT_D16_UNORM;

        // if we are multisampling then to enable copying of the depth buffer we have to enable a depth buffer resolve extension for vsg::RenderPass or require a minimum vulkan version of 1.2
        if (windowTraits->samples != VK_SAMPLE_COUNT_1_BIT) windowTraits->vulkanVersion = VK_API_VERSION_1_2;
        windowTraits->deviceExtensionNames = {
            VK_KHR_MULTIVIEW_EXTENSION_NAME,
            VK_KHR_MAINTENANCE2_EXTENSION_NAME,
            VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,
            VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME, 
            VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
            VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
            VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
            VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
            VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME};
        return windowTraits;
    }

public:

    void initRenderer(vsg::ref_ptr<vsg::Device> device)
    {
        auto options = vsg::Options::create();
        auto decodeWindowTraits = createWindowTraits("Decode", 4, options);

        decodeWindowTraits->device = device; //共享设备
        decode_window = vsg::Window::create(decodeWindowTraits);
        VkExtent2D extent = {};
        extent.width = render_width;
        extent.height = render_height;
        decode_screenshotHandler = ScreenshotHandler::create(decode_window, extent, DECODER);
        decode_viewer->addWindow(decode_window);
        auto decode_commandGraph = vsg::CommandGraph::create(decode_window);
        auto context = vsg::Context::create(decode_window->getOrCreateDevice());
        auto imageInfo = vsg::ImageInfo::create(vsg::ref_ptr<vsg::Sampler>{}, createImageView(*context, decode_screenshotHandler->m_encoder->output_image, VK_IMAGE_ASPECT_COLOR_BIT), VK_IMAGE_LAYOUT_GENERAL);
        auto copyImageViewToWindow = vsg::CopyImageViewToWindow::create(imageInfo->imageView, decode_window);
        decode_commandGraph->addChild(copyImageViewToWindow);
        decode_viewer->assignRecordAndSubmitTaskAndPresentation({decode_commandGraph});
        decode_viewer->compile();
        decode_viewer->addEventHandlers({vsg::CloseHandler::create(decode_viewer)});
    }

    void setWidthAndHeight(int width, int height, double scale){
        this->render_width = width * scale;
        this->render_height = height * scale;
    }

    bool render(std::vector<std::vector<uint8_t>> &vPacket){
        //--------------------------------------------------------------渲染循环----------------------------------------------------------//
        if (decode_viewer->advanceToNextFrame())
        {
            decode_screenshotHandler->decodeImage(decode_window, vPacket);

            decode_viewer->handleEvents(); //将保存在`UIEvents`对象中的事件传递给注册的事件处理器（`EventHandlers`）。通过调用这个函数，可以处理并响应窗口中发生的事件。
            decode_viewer->update();
            decode_viewer->recordAndSubmit(); //于记录和提交命令图。它会遍历`RecordAndSubmitTasks`列表中的任务，并对每个任务执行记录和提交操作。
            decode_viewer->present();
            return true;
        }
        else{
            return false;
        }
    }

    void getWindowImage(uint8_t* color){
        decode_screenshotHandler->screenshot_cpuimage(decode_window, color);
    }
};


