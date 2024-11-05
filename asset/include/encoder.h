#pragma  once
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
    NvDecoder* decoder;
    Cudactx* cudaContext;
    int mWidth;
    int mHeight;
    //NvEncoderInitParam encodeCLIOptions;
    NV_ENC_BUFFER_FORMAT eFormat = NV_ENC_BUFFER_FORMAT_ARGB;
    vsg::ref_ptr<vsg::Device> device;
    std::array<uint8_t, VK_UUID_SIZE> deviceUUID;
    vsg::ref_ptr<vsg::Window> window;
    void getDeviceUUID(vsg::Instance* instance, vsg::ref_ptr<vsg::Device> mDevice, std::array<uint8_t, VK_UUID_SIZE>& deviceUUID);

public:
    NvEncoderCuda* getEncoder() { return enc; }
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

    vsg::ref_ptr<vsg::Image> encode_destination_image;
    Cudaimage* encode_destination_cuimage;
    vsg::ref_ptr<vsg::Image> decode_image;
    Cudaimage* decode_cuimage;
    vsg::ref_ptr<vsg::Image> output_image;

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

        encode_destination_image = vsg::Image::create();
        encode_destination_image->imageType = VK_IMAGE_TYPE_2D;
        encode_destination_image->format = VK_FORMAT_R8G8B8A8_UNORM;
        encode_destination_image->extent.width = extent.width;
        encode_destination_image->extent.height = extent.height;
        encode_destination_image->extent.depth = 1;
        encode_destination_image->arrayLayers = 1;
        encode_destination_image->mipLevels = 1;
        encode_destination_image->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        encode_destination_image->samples = VK_SAMPLE_COUNT_1_BIT;
        encode_destination_image->tiling = VK_IMAGE_TILING_LINEAR;
        encode_destination_image->usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        VkExternalMemoryImageCreateInfo encodeExternalMemoryImageCreateInfo = {};
        encodeExternalMemoryImageCreateInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
        encodeExternalMemoryImageCreateInfo.pNext = nullptr;
        encodeExternalMemoryImageCreateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;
        encode_destination_image->pNext = &encodeExternalMemoryImageCreateInfo;
        VkExportMemoryAllocateInfo encodeExportMemoryAllocateInfo = {};
        encodeExportMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
        encodeExportMemoryAllocateInfo.pNext = nullptr;
        encodeExportMemoryAllocateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;
        encode_destination_image->pNextAllocInfo = &encodeExportMemoryAllocateInfo;
        encode_destination_image->compile(device);
        auto encodeDeviceMemory = vsg::DeviceMemory::create(device, encode_destination_image->getMemoryRequirements(device->deviceID), VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        encode_destination_image->bind(encodeDeviceMemory, 0);
        auto encodeBufferSize = encode_destination_image->getMemoryRequirements(device->deviceID).size;            
        std::cout << "bufferSize = " << encodeBufferSize << std::endl;
        encode_destination_cuimage = new Cudaimage(encode_destination_image, device, encodeBufferSize, extent);
    }

    void initDecoder(VkExtent2D extent) {
        mWidth = extent.width;
        mHeight = extent.height;
        decode_image = vsg::Image::create();
        decode_image->imageType = VK_IMAGE_TYPE_2D;
        decode_image->format = VK_FORMAT_R8G8B8A8_UNORM;
        decode_image->extent.width = extent.width;
        decode_image->extent.height = extent.height;
        decode_image->extent.depth = 1;
        decode_image->arrayLayers = 1;
        decode_image->mipLevels = 1;
        decode_image->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        decode_image->samples = VK_SAMPLE_COUNT_1_BIT;
        decode_image->tiling = VK_IMAGE_TILING_LINEAR;
        decode_image->usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        VkExternalMemoryImageCreateInfo decodeExternalMemoryImageCreateInfo = {};
        decodeExternalMemoryImageCreateInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
        decodeExternalMemoryImageCreateInfo.pNext = nullptr;
        decodeExternalMemoryImageCreateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;
        decode_image->pNext = &decodeExternalMemoryImageCreateInfo;
        VkExportMemoryAllocateInfo decodeExportMemoryAllocateInfo = {};
        decodeExportMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
        decodeExportMemoryAllocateInfo.pNext = nullptr;
        decodeExportMemoryAllocateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;
        decode_image->pNextAllocInfo = &decodeExportMemoryAllocateInfo;
        decode_image->compile(device);
        auto decodeDeviceMemory = vsg::DeviceMemory::create(device, decode_image->getMemoryRequirements(device->deviceID), VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        decode_image->bind(decodeDeviceMemory, 0);
        auto decodeBufferSize = decode_image->getMemoryRequirements(device->deviceID).size;            
        std::cout << "bufferSize = " << decodeBufferSize << std::endl;
        decode_cuimage = new Cudaimage(decode_image, device, decodeBufferSize, extent);


        output_image = vsg::Image::create();
        output_image->imageType = VK_IMAGE_TYPE_2D;
        output_image->format = VK_FORMAT_R8G8B8A8_UNORM;
        output_image->extent.width = extent.width;
        output_image->extent.height = extent.height;
        output_image->extent.depth = 1;
        output_image->arrayLayers = 1;
        output_image->mipLevels = 1;
        output_image->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        output_image->samples = VK_SAMPLE_COUNT_1_BIT;
        output_image->tiling = VK_IMAGE_TILING_LINEAR;
        output_image->usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        output_image->compile(device);
        auto deviceMemory = vsg::DeviceMemory::create(device, output_image->getMemoryRequirements(device->deviceID), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        output_image->bind(deviceMemory, 0);

        decoder = new NvDecoder(cudaContext->get(), true, cudaVideoCodec_H264);
    }

    void encode(std::vector<std::vector<uint8_t>>& vPacket)
    {
        // nRead = fpIn.read(reinterpret_cast<char*>(pHostFrame.get()), nFrameSize).gcount();
        const NvEncInputFrame* encoderInputFrame =  enc->GetNextInputFrame();
        CUdeviceptr encode_deviceptr = encode_destination_cuimage->get();

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
        // if (vPacket.size() > 0)
        //     std::cout << vPacket[0].size() << std::endl;
        // }
        // else 
        // {
        //     enc->EndEncode(vPacket);
        // }

        // std::ofstream fpOut("./a.h264", std::ios::app);
        // for (std::vector<uint8_t> &packet : vPacket)
        // {
        //     fpOut.write(reinterpret_cast<char*>(packet.data()), packet.size());
        // }
    }

    void decode(std::vector<std::vector<uint8_t>> &vPacket){
        CUdeviceptr decode_deviceptr = decode_cuimage->get();

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
    }
};