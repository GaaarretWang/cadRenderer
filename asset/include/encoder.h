#include "vsg/all.h"
#ifdef _WIN32
#include <vsg/platform/win32/Win32_Window.h>
#else
#include <vsg/platform/xcb/Xcb_Window.h>
#endif
#include <iostream>
#include <vector>
#include <cuda.h>
#include <nvEncodeAPI.h>
#include <NvEncoder.h>
#include <NvEncoderVK.h>
#include "NvDecoder.h"
#include <NvCodecUtils.h>
#include <vulkan/vulkan.h>
#include <cuda_runtime.h>

class Cudactx
{
    CUcontext m_context;

public:
    Cudactx(std::array<uint8_t, VK_UUID_SIZE>& deviceUUID);
    ~Cudactx() {};

    CUresult memcpyDtoH(void* p, CUdeviceptr dptr, size_t size);

    CUresult memcpy2D(void* p, CUarray array, uint32_t width, uint32_t height);

    CUcontext get() const
    {
        return m_context;
    }
};

class Vkimgmembarrier
{
    VkImageMemoryBarrier m_barrier;

public:
    Vkimgmembarrier(vsg::ref_ptr<vsg::Image> image, uint32_t deviceID);
    ~Vkimgmembarrier() {};

    VkImageMemoryBarrier get() const
    {
        return m_barrier;
    }
};

class Cudaimage
{
    CUarray m_array;
    CUmipmappedArray m_mipmapArray;
    CUexternalMemory m_extMem;

public:
    Cudaimage(vsg::ref_ptr<vsg::Image> image, vsg::ref_ptr<vsg::Device> m_device, VkDeviceSize deviceSize, VkExtent2D extent);
    ~Cudaimage();

    CUarray get() const
    {
        return m_array;
    }
};

class Cudasema
{
    CUexternalSemaphore m_extSema;

public:
    Cudasema(vsg::ref_ptr<vsg::Semaphore> semaphore, vsg::ref_ptr<vsg::Device> m_device);
    ~Cudasema();

    CUresult wait(void);
    CUresult signal(void);
};

struct DeviceAlloc
{
    vsg::ref_ptr<vsg::Image> vulkanImage;
    vsg::ref_ptr<vsg::Semaphore> vulkanSemaphore;
    Vkimgmembarrier* preOpBarrier;
    Vkimgmembarrier* postOpBarrier;
    Cudaimage* cudaImage;
    Cudasema* cudaSemaphore;
};

class NvEncoderWrapper {
private:
    NvEncoderVK* encoder;
    NvDecoder* decoder;
    Cudactx* cudaContext;
    int mWidth;
    int mHeight;
    //NvEncoderInitParam encodeCLIOptions;
    NV_ENC_BUFFER_FORMAT eFormat = NV_ENC_BUFFER_FORMAT_ARGB;
    std::vector<DeviceAlloc> encodeSurfaces;
    std::vector<DeviceAlloc> decodeSurfaces;
    vsg::ref_ptr<vsg::Device> device;
    vsg::ref_ptr<vsg::Buffer> m_readBackBuffer;
    std::array<uint8_t, VK_UUID_SIZE> deviceUUID;
    vsg::ref_ptr<vsg::Window> window;
    void getDeviceUUID(vsg::Instance* instance, vsg::ref_ptr<vsg::Device> mDevice, std::array<uint8_t, VK_UUID_SIZE>& deviceUUID);

public:
    NvEncoderVK* getEncoder() { return encoder; }
    NvDecoder* getDecoder() { return decoder; }
    Cudactx* getCudaContext() { return cudaContext; }
    std::map<CUarray, DeviceAlloc*> mapCUarrayToDeviceAlloc;

    void initCuda(vsg::Instance* instance, vsg::ref_ptr<vsg::Window> window)
    {
        device = window->getOrCreateDevice();
        getDeviceUUID(instance, device, deviceUUID);
        cudaContext = new Cudactx(deviceUUID);
        std::cout << "Cuda Init succeed!" << std::endl;
        this->window = window;
    }

    void initEncoder(VkExtent2D extent)
    {
        mWidth = extent.width;
        mHeight = extent.height;
        encoder = new NvEncoderVK(cudaContext->get(), mWidth, mHeight, eFormat);
        NV_ENC_INITIALIZE_PARAMS initializeParams = { NV_ENC_INITIALIZE_PARAMS_VER };
        NV_ENC_CONFIG encodeConfig = { NV_ENC_CONFIG_VER };
        initializeParams.encodeConfig = &encodeConfig;
        std::ostringstream oss;
        //encodeCLIOptions = NvEncoderInitParam(oss.str().c_str());
        encoder->CreateDefaultEncoderParams(&initializeParams,
            NV_ENC_CODEC_H264_GUID,
            NV_ENC_PRESET_P3_GUID,
            NV_ENC_TUNING_INFO_ULTRA_LOW_LATENCY);
        //encodeCLIOptions.SetInitParams(&initializeParams, eFormat);

        encoder->CreateEncoder(&initializeParams);

        int32_t numInputBuffers = encoder->GetNumInputBuffers();
        std::cout << "numInputBuffers:" << numInputBuffers << std::endl;
        encodeSurfaces.resize(numInputBuffers);

        for (int i = 0; i < numInputBuffers; i++)
        {
            encodeSurfaces[i].vulkanImage = vsg::Image::create();
            encodeSurfaces[i].vulkanImage->imageType = VK_IMAGE_TYPE_2D;
            encodeSurfaces[i].vulkanImage->format = VK_FORMAT_B8G8R8A8_SRGB; //VK_FORMAT_R8G8B8A8_UNORM;
            encodeSurfaces[i].vulkanImage->extent.width = extent.width;
            encodeSurfaces[i].vulkanImage->extent.height = extent.height;
            encodeSurfaces[i].vulkanImage->extent.depth = 1;
            encodeSurfaces[i].vulkanImage->mipLevels = 1;
            encodeSurfaces[i].vulkanImage->arrayLayers = 1;
            encodeSurfaces[i].vulkanImage->samples = VK_SAMPLE_COUNT_1_BIT;
            encodeSurfaces[i].vulkanImage->tiling = VK_IMAGE_TILING_OPTIMAL;
            encodeSurfaces[i].vulkanImage->usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT; // sample和storage一定要标
            encodeSurfaces[i].vulkanImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            VkExternalMemoryImageCreateInfo externalMemoryImageCreateInfo = {};
            externalMemoryImageCreateInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
            externalMemoryImageCreateInfo.pNext = nullptr;
            externalMemoryImageCreateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
            encodeSurfaces[i].vulkanImage->pNext = &externalMemoryImageCreateInfo;

            VkExportMemoryAllocateInfo exportMemoryAllocateInfo = {};
            exportMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
            exportMemoryAllocateInfo.pNext = nullptr;
            exportMemoryAllocateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHR;
            encodeSurfaces[i].vulkanImage->pNextAllocInfo = &exportMemoryAllocateInfo;

            //auto context = vsg::Context::create(window->getOrCreateDevice());

            encodeSurfaces[i].vulkanImage->compile(device);
            encodeSurfaces[i].vulkanImage->allocateAndBindMemory(device);

            VkExportSemaphoreCreateInfoKHR exportSemaphoreCreateInfo = {};
            exportSemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO_KHR;
            exportSemaphoreCreateInfo.pNext = nullptr;
            exportSemaphoreCreateInfo.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;
            encodeSurfaces[i].vulkanSemaphore = vsg::Semaphore::create(device, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, &exportSemaphoreCreateInfo);
        }

        for (int i = 0; i < numInputBuffers; i++)
        {
            vsg::ref_ptr<vsg::Image> image = encodeSurfaces[i].vulkanImage;

            encodeSurfaces[i].preOpBarrier = new Vkimgmembarrier(image, device->deviceID);
            encodeSurfaces[i].postOpBarrier = new Vkimgmembarrier(image, device->deviceID);
        }

        for (int i = 0; i < numInputBuffers; i++)
        {
            auto bufferSize = encodeSurfaces[i].vulkanImage->getMemoryRequirements(device->deviceID).size;            
            Cudaimage* cuImage = new Cudaimage(encodeSurfaces[i].vulkanImage,
                device, bufferSize, extent);
            Cudasema* cuSema = new Cudasema(encodeSurfaces[i].vulkanSemaphore, device);
            encodeSurfaces[i].cudaImage = cuImage;
            encodeSurfaces[i].cudaSemaphore = cuSema;
            mapCUarrayToDeviceAlloc[cuImage->get()] = &encodeSurfaces[i];
        }

        std::vector<void*> inputFrames;
        for (int i = 0; i < numInputBuffers; i++)
            inputFrames.push_back((void*)encodeSurfaces[i].cudaImage->get()); //(void*)encodeSurfaces[i].cudaImage->get() CUarray!!!
        encoder->RegisterInputResources(inputFrames, NV_ENC_INPUT_RESOURCE_TYPE_CUDAARRAY,
            mWidth, mHeight, mWidth, eFormat);
    }

    void initDecoder() {
        decoder = new NvDecoder(cudaContext->get(), true, cudaVideoCodec_H264);
    }
    //MFW::Wrapper::Buffer::Ptr readBackVulkanBuffer(CUdeviceptr dpFrame);
};