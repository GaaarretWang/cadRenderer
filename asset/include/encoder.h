#include "vsg/all.h"
#ifdef _WIN32
#include <vsg/platform/win32/Win32_Window.h>
#else
#include <vsg/platform/xcb/Xcb_Window.h>
#endif
#include <iostream>
#include <vector>
#include <nvEncodeAPI.h>
#include <NvEncoder.h>
#include <NvEncoderVK.h>
#include "NvDecoder.h"
#include <NvCodecUtils.h>
#include <vulkan/vulkan.h>
#include <cuda.h>
// #include <cuda_runtime_api.h>
#include <cuda_runtime.h>

#include "NvEncoderCuda.h"
#include "ColorSpace.h"

// #include "NvEncoderCLIOptions.h"

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
    CUexternalMemory m_extMem;
    CUdeviceptr m_deviceptr;

public:
    Cudaimage(vsg::ref_ptr<vsg::Image> image, vsg::ref_ptr<vsg::Device> m_device, VkDeviceSize deviceSize, VkExtent2D extent);
    ~Cudaimage();

    CUdeviceptr get() const
    {
        return m_deviceptr;
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
    NvEncoderCuda* enc;

    void initCuda(vsg::Instance* instance, vsg::ref_ptr<vsg::Window> window)
    {
        device = window->getOrCreateDevice();
        getDeviceUUID(instance, device, deviceUUID);
        cudaContext = new Cudactx(deviceUUID);
        std::cout << "Cuda Init succeed!" << std::endl;
        this->window = window;
    }

    vsg::ref_ptr<vsg::Image> encodeDestinationImage;
    Cudaimage* encodeDestinationCUImage;
    vsg::ref_ptr<vsg::Image> decodeImage;
    Cudaimage* decodeCUImage;
    CUdeviceptr dpFrame = 0;

    void initEncoder(VkExtent2D extent)
    {
        mWidth = extent.width;
        mHeight = extent.height;
        
        // NvEncoderInitParam encodeCLIOptions;
        // NvEncoderInitParam* pEncodeCLIOptions = &encodeCLIOptions;
        CUcontext cuContext = cudaContext->get();
        enc = new NvEncoderCuda(cuContext, extent.width, extent.height, eFormat, 0);
        std::cout << extent.width << extent.height << std::endl;
        NV_ENC_INITIALIZE_PARAMS initializeParams = { NV_ENC_INITIALIZE_PARAMS_VER };
        NV_ENC_CONFIG encodeConfig = { NV_ENC_CONFIG_VER };
        initializeParams.encodeConfig = &encodeConfig;
        enc->CreateDefaultEncoderParams(&initializeParams, NV_ENC_CODEC_H264_GUID, NV_ENC_PRESET_P3_GUID,
                    NV_ENC_TUNING_INFO_ULTRA_LOW_LATENCY);
        initializeParams.encodeWidth = extent.width;                     /**< [in]: Specifies the encode width. If not set ::NvEncInitializeEncoder() API will fail. */
        initializeParams.encodeHeight = extent.height;                    /**< [in]: Specifies the encode height. If not set ::NvEncInitializeEncoder() API will fail. */

        encodeConfig.gopLength = NVENC_INFINITE_GOPLENGTH;
        encodeConfig.frameIntervalP = 1;
        encodeConfig.encodeCodecConfig.h264Config.idrPeriod = NVENC_INFINITE_GOPLENGTH;
        encodeConfig.rcParams.rateControlMode = NV_ENC_PARAMS_RC_CBR;
        encodeConfig.rcParams.multiPass = NV_ENC_TWO_PASS_FULL_RESOLUTION;
        // encodeConfig.rcParams.averageBitRate = (static_cast<unsigned int>(5.0f * initializeParams.encodeWidth * initializeParams.encodeHeight) / (1280 * 720)) * 100000;
        // encodeConfig.rcParams.vbvBufferSize = (encodeConfig.rcParams.averageBitRate * initializeParams.frameRateDen / initializeParams.frameRateNum) * 5;
        encodeConfig.rcParams.maxBitRate = encodeConfig.rcParams.averageBitRate;
        encodeConfig.rcParams.vbvInitialDelay = encodeConfig.rcParams.vbvBufferSize;
        // pEncodeCLIOptions->SetInitParams(&initializeParams, eFormat);
        enc->CreateEncoder(&initializeParams);

        encodeDestinationImage = vsg::Image::create();
        encodeDestinationImage->imageType = VK_IMAGE_TYPE_2D;
        encodeDestinationImage->format = VK_FORMAT_R8G8B8A8_UNORM;
        encodeDestinationImage->extent.width = extent.width;
        encodeDestinationImage->extent.height = extent.height;
        encodeDestinationImage->extent.depth = 1;
        encodeDestinationImage->arrayLayers = 1;
        encodeDestinationImage->mipLevels = 1;
        encodeDestinationImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        encodeDestinationImage->samples = VK_SAMPLE_COUNT_1_BIT;
        encodeDestinationImage->tiling = VK_IMAGE_TILING_LINEAR;
        encodeDestinationImage->usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        VkExternalMemoryImageCreateInfo encodeExternalMemoryImageCreateInfo = {};
        encodeExternalMemoryImageCreateInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
        encodeExternalMemoryImageCreateInfo.pNext = nullptr;
        encodeExternalMemoryImageCreateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;
        encodeDestinationImage->pNext = &encodeExternalMemoryImageCreateInfo;
        VkExportMemoryAllocateInfo encodeExportMemoryAllocateInfo = {};
        encodeExportMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
        encodeExportMemoryAllocateInfo.pNext = nullptr;
        encodeExportMemoryAllocateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;
        encodeDestinationImage->pNextAllocInfo = &encodeExportMemoryAllocateInfo;
        encodeDestinationImage->compile(device);
        auto encodeDeviceMemory = vsg::DeviceMemory::create(device, encodeDestinationImage->getMemoryRequirements(device->deviceID), VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        encodeDestinationImage->bind(encodeDeviceMemory, 0);
        auto encodeBufferSize = encodeDestinationImage->getMemoryRequirements(device->deviceID).size;            
        std::cout << "bufferSize = " << encodeBufferSize << std::endl;
        encodeDestinationCUImage = new Cudaimage(encodeDestinationImage, device, encodeBufferSize, extent);

        decodeImage = vsg::Image::create();
        decodeImage->imageType = VK_IMAGE_TYPE_2D;
        decodeImage->format = VK_FORMAT_R8G8B8A8_UNORM;
        decodeImage->extent.width = extent.width;
        decodeImage->extent.height = extent.height;
        decodeImage->extent.depth = 1;
        decodeImage->arrayLayers = 1;
        decodeImage->mipLevels = 1;
        decodeImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        decodeImage->samples = VK_SAMPLE_COUNT_1_BIT;
        decodeImage->tiling = VK_IMAGE_TILING_LINEAR;
        decodeImage->usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        VkExternalMemoryImageCreateInfo decodeExternalMemoryImageCreateInfo = {};
        decodeExternalMemoryImageCreateInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
        decodeExternalMemoryImageCreateInfo.pNext = nullptr;
        decodeExternalMemoryImageCreateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;
        decodeImage->pNext = &decodeExternalMemoryImageCreateInfo;
        VkExportMemoryAllocateInfo decodeExportMemoryAllocateInfo = {};
        decodeExportMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
        decodeExportMemoryAllocateInfo.pNext = nullptr;
        decodeExportMemoryAllocateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;
        decodeImage->pNextAllocInfo = &decodeExportMemoryAllocateInfo;
        decodeImage->compile(device);
        auto decodeDeviceMemory = vsg::DeviceMemory::create(device, decodeImage->getMemoryRequirements(device->deviceID), VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        decodeImage->bind(decodeDeviceMemory, 0);
        auto decodeBufferSize = decodeImage->getMemoryRequirements(device->deviceID).size;            
        std::cout << "bufferSize = " << decodeBufferSize << std::endl;
        decodeCUImage = new Cudaimage(decodeImage, device, decodeBufferSize, extent);

        cuMemAlloc(&dpFrame, extent.width * extent.height * 4);
    }

    void encodeAndDecode()
    {
        std::vector<std::vector<uint8_t>> vPacket;
        // nRead = fpIn.read(reinterpret_cast<char*>(pHostFrame.get()), nFrameSize).gcount();
        const NvEncInputFrame* encoderInputFrame =  enc->GetNextInputFrame();
        CUdeviceptr encode_deviceptr = encodeDestinationCUImage->get();

        CUcontext cuContext = cudaContext->get();
        NvEncoderCuda::CopyToDeviceFrame(cuContext,
            (void*)encode_deviceptr,
            0, 
            (CUdeviceptr)encoderInputFrame->inputPtr,
            (int)encoderInputFrame->pitch,
            enc->GetEncodeWidth(),
            enc->GetEncodeHeight(),
            CU_MEMORYTYPE_DEVICE, 
            encoderInputFrame->bufferFormat,
            encoderInputFrame->chromaOffsets,
            encoderInputFrame->numChromaPlanes);
        NV_ENC_PIC_PARAMS picParams = {NV_ENC_PIC_PARAMS_VER};
        picParams.encodePicFlags = 0;

        enc->EncodeFrame(vPacket, &picParams);
        // }
        // else 
        // {
        //     enc->EndEncode(vPacket);
        // }
        std::cout << vPacket[0].size() << std::endl;
        // std::ofstream fpOut("./a.h264", std::ios::app);
        // for (std::vector<uint8_t> &packet : vPacket)
        // {
        //     fpOut.write(reinterpret_cast<char*>(packet.data()), packet.size());
        // }
        CUdeviceptr decode_deviceptr = decodeCUImage->get();

        if (vPacket.size() > 0) {
            int nFrameReturned = decoder->Decode(vPacket[0].data(), vPacket[0].size());
            for (int i = 0; i < nFrameReturned; i++)
            {
                int64_t timestamp = 0;
                uint8_t* pFrame = decoder->GetFrame(&timestamp);
                int iMatrix = decoder->GetVideoFormatInfo().video_signal_description.matrix_coefficients;
                if (decoder->GetBitDepth() == 8)
                {
                    if (decoder->GetOutputFormat() == cudaVideoSurfaceFormat_YUV444)
                        YUV444ToColor32<BGRA32>(pFrame, decoder->GetWidth(), (uint8_t*)decode_deviceptr, 4 * mWidth, decoder->GetWidth(), decoder->GetHeight(), iMatrix);
                    else    // default assumed as NV12
                        Nv12ToColor32<BGRA32>(pFrame, decoder->GetWidth(), (uint8_t*)decode_deviceptr, 4 * mWidth, decoder->GetWidth(), decoder->GetHeight(), iMatrix);
                }
                else
                {
                    if (decoder->GetOutputFormat() == cudaVideoSurfaceFormat_YUV444_16Bit)
                        YUV444P16ToColor32<BGRA32>(pFrame, decoder->GetWidth(), (uint8_t*)decode_deviceptr, 4 * mWidth, decoder->GetWidth(), decoder->GetHeight(), iMatrix);
                    else // default assumed as P016
                        P016ToColor32<BGRA32>(pFrame, decoder->GetWidth(), (uint8_t*)decode_deviceptr, 4 * mWidth, decoder->GetWidth(), decoder->GetHeight(), iMatrix);
                }
            }
        }

        // CUDA_MEMCPY2D copy = {};
        // copy.srcMemoryType = CU_MEMORYTYPE_DEVICE;
        // copy.srcDevice = dpFrame;
        // copy.dstMemoryType = CU_MEMORYTYPE_DEVICE;
        // copy.dstDevice = decode_deviceptr;
        // copy.dstPitch = mWidth * 4;
        // copy.WidthInBytes = mWidth * 4;
        // copy.Height = mHeight;

        // CUresult result = cuMemcpy2D(&copy);
        // if (result != CUDA_SUCCESS) {
        //     throw std::runtime_error("Failed cuArrayCreate");
        // }

        // CUDA_MEMCPY2D copy = {};
        // void* hostData = malloc(mWidth * mHeight * 4);
        // copy.srcMemoryType = CU_MEMORYTYPE_DEVICE;
        // copy.srcDevice = decode_deviceptr;
        // copy.dstMemoryType = CU_MEMORYTYPE_HOST;
        // copy.dstHost = hostData;
        // copy.WidthInBytes = 4 * mWidth;
        // copy.Height = mHeight;

        // CUresult result = cuMemcpy2D(&copy);
        // if (result != CUDA_SUCCESS) {
        //     const char* errorString;
        //     cuGetErrorString(result, &errorString);
        //     printf("CUDA Error: %s\n", errorString);

        //     throw std::runtime_error("Failed cuMemcpy2D");
        // }
        // for(int m = 0; m < 100; m ++)
        //     std::cout << (int)(*(reinterpret_cast<uint8_t*>(hostData) + m)) << " ";

    }

    void initDecoder() {
        decoder = new NvDecoder(cudaContext->get(), true, cudaVideoCodec_H264);
    }
    //MFW::Wrapper::Buffer::Ptr readBackVulkanBuffer(CUdeviceptr dpFrame);
};