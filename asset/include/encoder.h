#pragma  once
#include "vsg/all.h"
#ifdef _WIN32
#include "Logger.h"
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
#include "upscaleImage.h"
// #include "NvEncoderCLIOptions.h"

/**
 * CUDA上下文管理类
 * 核心功能：创建与Vulkan物理设备匹配的CUDA context
 * 匹配原理：通过设备UUID在CUDA设备列表中找到与Vulkan相同的GPU，
 *           因为跨API（Vulkan和CUDA）没有其他方式来确保使用同一块显卡
 */
class Cudactx
{
    CUcontext m_context;  // CUDA上下文句柄，相当于CUDA的"设备会话"

public:
    // 传入Vulkan设备的UUID，在CUDA设备中查找匹配的GPU并创建context
    Cudactx(std::array<uint8_t, VK_UUID_SIZE>& deviceUUID);
    ~Cudactx() {};

    // 设备到主机内存拷贝（Device-to-Host）：将GPU显存数据拷贝到CPU内存
    CUresult memcpyDtoH(void* p, CUdeviceptr dptr, size_t size);

    // 二维内存拷贝：从CUarray（CUDA数组）拷贝到主机内存，支持按行pitch对齐
    CUresult memcpy2D(void* p, CUarray array, uint32_t width, uint32_t height);

    // 获取底层CUDA context句柄，供NvEncoder等组件使用
    CUcontext get() const
    {
        return m_context;
    }
};

/**
 * Vulkan图像内存屏障（Image Memory Barrier）辅助类
 * 用途：在Vulkan管线中控制图像的layout转换和队列族所有权转移
 *       例如：将图像从COLOR_ATTACHMENT_OPTIMAL转为TRANSFER_SRC_OPTIMAL以便CUDA读取
 * 注意：设置了VK_QUEUE_FAMILY_IGNORED，表示不做队列族转移
 */
class Vkimgmembarrier
{
    VkImageMemoryBarrier m_barrier;  // 底层Vulkan屏障结构体

public:
    // 构造时自动配置subresource（颜色通道，mip level 0，layer 0）
    Vkimgmembarrier(vsg::ref_ptr<vsg::Image> image, uint32_t deviceID);
    ~Vkimgmembarrier() {};

    VkImageMemoryBarrier get() const
    {
        return m_barrier;
    }
};

/**
 * CUDA外部图像类 —— 将Vulkan Image的显存导入CUDA
 *
 * 工作流程（Vulkan-CUDA零拷贝互操作的核心）：
 *   1. 获取Vulkan Image的底层显存句柄（Win32 HANDLE 或 Linux fd）
 *   2. 通过cuImportExternalMemory导入到CUDA，得到CUexternalMemory
 *   3. 通过cuExternalMemoryGetMappedBuffer映射为CUdeviceptr
 *
 * 效果：CUDA可以直接通过CUdeviceptr读写Vulkan的Image内存，无需CPU参与拷贝
 */
class Cudaimage
{
    CUexternalMemory m_extMem;   // CUDA外部内存对象（从Vulkan导入的显存）
    CUdeviceptr m_deviceptr;     // CUDA设备指针，指向Vulkan显存的起始地址，实现零拷贝

public:
    // image: Vulkan图像, device: Vulkan逻辑设备, deviceSize: 显存大小(字节), extent: 图像尺寸
    Cudaimage(vsg::ref_ptr<vsg::Image> image, vsg::ref_ptr<vsg::Device> m_device, VkDeviceSize deviceSize, VkExtent2D extent);
    ~Cudaimage();

    // 返回CUDA设备指针，调用方可以直接用它做CUDA kernel计算或内存拷贝
    CUdeviceptr get() const
    {
        return m_deviceptr;
    }
};

/**
 * CUDA外部信号量类 —— 将Vulkan Semaphore导入CUDA，实现跨API GPU同步
 *
 * 为什么需要：Vulkan渲染完一帧后，CUDA需要等待Vulkan完成才能安全读取Image数据；
 *           反之CUDA处理完后，也需要通知Vulkan可以继续使用该Image。
 *           没有信号量同步会导致画面撕裂或数据竞争。
 *
 * 工作流程：
 *   1. 获取Vulkan Semaphore的底层句柄（Win32 HANDLE 或 Linux fd）
 *   2. 通过cuImportExternalSemaphore导入到CUDA
 *   3. 调用wait()阻塞CUDA流直到Vulkan完成，调用signal()通知Vulkan可以继续
 */
class Cudasema
{
    CUexternalSemaphore m_extSema;  // CUDA外部信号量对象（从Vulkan导入）

public:
    // 从Vulkan信号量构造CUDA外部信号量
    Cudasema(vsg::ref_ptr<vsg::Semaphore> semaphore, vsg::ref_ptr<vsg::Device> m_device);
    ~Cudasema();

    // CUDA侧等待：阻塞CUDA操作直到Vulkan侧signal该信号量（等价于"等Vulkan画完"）
    CUresult wait(void);
    // CUDA侧通知：告诉Vulkan侧CUDA已处理完，Vulkan可以继续（等价于"CUDA处理完了"）
    CUresult signal(void);
};

/**
 * 渲染目标资源包 —— 将一个Vulkan Image所需的互操作资源打包在一起
 *
 * 每个渲染目标（如深度图、颜色图）都有一套Vulkan侧资源和CUDA侧资源：
 *   - Vulkan侧：Image + Semaphore + 前后屏障（barrier）
 *   - CUDA侧：导入的外部Image + 导入的外部Semaphore
 *
 * 典型流程：Vulkan渲染 -> preOpBarrier转换layout -> CUDA wait semaphore -> CUDA处理
 *          -> CUDA signal semaphore -> postOpBarrier恢复layout -> Vulkan继续
 */
struct DeviceAlloc
{
    vsg::ref_ptr<vsg::Image> vulkanImage;          // Vulkan图像资源
    vsg::ref_ptr<vsg::Semaphore> vulkanSemaphore;  // Vulkan信号量（用于Vulkan-CUDA同步）
    Vkimgmembarrier* preOpBarrier;   // CUDA操作前的屏障：将Image layout转为CUDA可读写状态
    Vkimgmembarrier* postOpBarrier;  // CUDA操作后的屏障：将Image layout恢复为Vulkan可用状态
    Cudaimage* cudaImage;            // 导入CUDA的外部图像（零拷贝访问Vulkan显存）
    Cudasema* cudaSemaphore;         // 导入CUDA的外部信号量（跨API同步）
};

/**
 * NVENC硬件编解码器封装类（核心调度类）
 *
 * 职责：
 *   1. 初始化CUDA上下文，并确保CUDA与Vulkan使用同一块GPU（通过UUID匹配）
 *   2. 管理H.264硬件编码（NVENC）和解码（NVDEC）
 *   3. 创建可被CUDA访问的Vulkan图像（通过外部内存互操作）
 *
 * 互操作架构：
 *   Vulkan渲染 -> Vulkan Image（显存） -> CUDA零拷贝访问 -> NVENC编码 -> 网络传输
 *   网络接收 -> NVDEC解码 -> CUDA设备指针 -> Vulkan Image -> 显示
 */
class NvEncoderWrapper {
private:
    NvDecoder* decoder;        // NVDEC硬件解码器
    Cudactx* cudaContext;      // CUDA上下文（与Vulkan匹配的GPU）
    int mWidth;                // 图像宽度（像素）
    int mHeight;               // 图像高度（像素）
    //NvEncoderInitParam encodeCLIOptions;
    NV_ENC_BUFFER_FORMAT eFormat = NV_ENC_BUFFER_FORMAT_ARGB;  // 编码输入格式：32位ARGB
    vsg::ref_ptr<vsg::Device> device;                          // Vulkan逻辑设备
    std::array<uint8_t, VK_UUID_SIZE> deviceUUID;              // Vulkan设备的UUID（16字节），用于匹配CUDA设备
    vsg::ref_ptr<vsg::Window> window;                          // 窗口句柄
    // 获取Vulkan物理设备的UUID（通过vkGetPhysicalDeviceProperties2KHR扩展查询）
    void getDeviceUUID(vsg::Instance* instance, vsg::ref_ptr<vsg::Device> mDevice, std::array<uint8_t, VK_UUID_SIZE>& deviceUUID);

public:
    NvEncoderCuda* getEncoder() { return enc; }       // 获取编码器实例
    NvDecoder* getDecoder() { return decoder; }        // 获取解码器实例
    Cudactx* getCudaContext() { return cudaContext; }  // 获取CUDA上下文
    NvEncoderCuda* enc;  // NVENC编码器（使用CUDA作为输入源）

    // 初始化CUDA：获取Vulkan设备UUID -> 匹配CUDA设备 -> 创建CUDA context
    void initCuda(vsg::Instance* instance, vsg::ref_ptr<vsg::Window> window)
    {
        device = window->getOrCreateDevice();
        getDeviceUUID(instance, device, deviceUUID);
        cudaContext = new Cudactx(deviceUUID);
        std::cout << "Cuda Init succeed!" << std::endl;
        this->window = window;
    }

    // 编码用的Vulkan图像及其CUDA映射（零拷贝：Vulkan渲染结果直接被NVENC读取）
    vsg::ref_ptr<vsg::Image> encode_destination_image;  // Vulkan侧：编码输入图像
    Cudaimage* encode_destination_cuimage;               // CUDA侧：导入的外部图像，获取CUdeviceptr
    // 解码用的Vulkan图像及其CUDA映射（零拷贝：NVDEC解码结果直接写入Vulkan图像）
    vsg::ref_ptr<vsg::Image> decode_image;  // Vulkan侧：解码输出图像
    Cudaimage* decode_cuimage;               // CUDA侧：导入的外部图像
    vsg::ref_ptr<vsg::Image> output_image;  // 主机可见的输出图像（用于CPU回读显示）
    // 纯CUDA设备内存（用于图像分辨率缩放，当编码分辨率!=渲染分辨率时使用）
    CUdeviceptr d_cuda_input = 0;   // 缩放输入缓冲区（CUDA device memory）
    CUdeviceptr d_cuda_output = 0;  // 缩放输出缓冲区（CUDA device memory）
    VkExtent2D m_extent;            // 渲染分辨率（原始图像尺寸）
    VkExtent2D m_encode_extent;     // 编码分辨率（可能与渲染分辨率不同，用于降低带宽）

    // 初始化编码器：创建NVENC编码器 + 创建可被CUDA访问的Vulkan Image（外部内存）
    // extent: 渲染分辨率, encode_extent: 编码分辨率
    void initEncoder(VkExtent2D extent, VkExtent2D encode_extent)
    {
        mWidth = extent.width;
        mHeight = extent.height;
        m_extent = extent;
        m_encode_extent = encode_extent;
        // NvEncoderInitParam encodeCLIOptions;
        // NvEncoderInitParam* pEncodeCLIOptions = &encodeCLIOptions;
        CUcontext cuContext = cudaContext->get();
        enc = new NvEncoderCuda(cuContext, encode_extent.width, encode_extent.height, eFormat, 0);
        std::cout << encode_extent.width << encode_extent.height << std::endl;
        NV_ENC_INITIALIZE_PARAMS initializeParams = { NV_ENC_INITIALIZE_PARAMS_VER };
        NV_ENC_CONFIG encodeConfig = { NV_ENC_CONFIG_VER };
        initializeParams.encodeConfig = &encodeConfig;
        enc->CreateDefaultEncoderParams(&initializeParams, NV_ENC_CODEC_H264_GUID, NV_ENC_PRESET_P1_GUID,
                    NV_ENC_TUNING_INFO_ULTRA_LOW_LATENCY);
        initializeParams.encodeWidth = encode_extent.width;                     /**< [in]: Specifies the encode width. If not set ::NvEncInitializeEncoder() API will fail. */
        initializeParams.encodeHeight = encode_extent.height;                    /**< [in]: Specifies the encode height. If not set ::NvEncInitializeEncoder() API will fail. */

        encodeConfig.gopLength = NVENC_INFINITE_GOPLENGTH;
        encodeConfig.frameIntervalP = 1;
        encodeConfig.encodeCodecConfig.h264Config.idrPeriod = NVENC_INFINITE_GOPLENGTH;
        encodeConfig.rcParams.rateControlMode = NV_ENC_PARAMS_RC_CBR;
        encodeConfig.rcParams.multiPass = NV_ENC_MULTI_PASS_DISABLED;
        encodeConfig.rcParams.averageBitRate = static_cast<unsigned int>(3.0 * 1024 * 1024);
        // encodeConfig.rcParams.vbvBufferSize = (encodeConfig.rcParams.averageBitRate * initializeParams.frameRateDen / initializeParams.frameRateNum) * 5;
        encodeConfig.rcParams.maxBitRate = encodeConfig.rcParams.averageBitRate;
        // encodeConfig.rcParams.vbvInitialDelay = encodeConfig.rcParams.vbvBufferSize;
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
        encodeExternalMemoryImageCreateInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_KHR;
        encodeExternalMemoryImageCreateInfo.pNext = nullptr;
        #ifdef _WIN32
        encodeExternalMemoryImageCreateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHR;
        #else
        encodeExternalMemoryImageCreateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;
        #endif
        encode_destination_image->pNext = &encodeExternalMemoryImageCreateInfo;
        VkExportMemoryAllocateInfo encodeExportMemoryAllocateInfo = {};
        encodeExportMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
        encodeExportMemoryAllocateInfo.pNext = nullptr;
        #ifdef _WIN32
        encodeExportMemoryAllocateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHR;
        #else
        encodeExportMemoryAllocateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;
        #endif
        encode_destination_image->pNextAllocInfo = &encodeExportMemoryAllocateInfo;
        encode_destination_image->compile(device);
        auto encodeDeviceMemory = vsg::DeviceMemory::create(device, encode_destination_image->getMemoryRequirements(device->deviceID), VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &encodeExportMemoryAllocateInfo);
        encode_destination_image->bind(encodeDeviceMemory, 0);
        auto encodeBufferSize = encode_destination_image->getMemoryRequirements(device->deviceID).size;            
        std::cout << "bufferSize = " << encodeBufferSize << std::endl;
        encode_destination_cuimage = new Cudaimage(encode_destination_image, device, encodeBufferSize, extent);

        size_t cuda_input_size = static_cast<size_t>(extent.width) * extent.height * 4;
        cudaMalloc((void**)&d_cuda_input, cuda_input_size);
        size_t cuda_output_size = static_cast<size_t>(encode_extent.width) * encode_extent.height * 4;
        cudaMalloc((void**)&d_cuda_output, cuda_output_size);
    }

    // 初始化解码器：创建NVDEC解码器 + 创建可被CUDA访问的Vulkan Image
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
        #ifdef _WIN32
        decodeExternalMemoryImageCreateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHR;
        #else
        decodeExternalMemoryImageCreateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;
        #endif
        decode_image->pNext = &decodeExternalMemoryImageCreateInfo;
        VkExportMemoryAllocateInfo decodeExportMemoryAllocateInfo = {};
        decodeExportMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
        decodeExportMemoryAllocateInfo.pNext = nullptr;
        #ifdef _WIN32
        decodeExportMemoryAllocateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHR;
        #else
        decodeExportMemoryAllocateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;
        #endif
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

    // 编码一帧：Vulkan Image (CUDA零拷贝) -> 如果需要缩放 -> CopyToDeviceFrame -> NVENC H.264编码
    // vPacket: 输出的H.264码流包列表
    void encode(std::vector<std::vector<uint8_t>>& vPacket)
    {
        // nRead = fpIn.read(reinterpret_cast<char*>(pHostFrame.get()), nFrameSize).gcount();
        const NvEncInputFrame* encoderInputFrame =  enc->GetNextInputFrame();
        CUdeviceptr encode_deviceptr = encode_destination_cuimage->get();
        void* encode_input = (void*)encode_deviceptr;
        if(m_extent.width != m_encode_extent.width && m_extent.height != m_encode_extent.height){
            cudaMemcpy(
                (void*)d_cuda_input,        // 目标：CUDA 输入内存
                (void*)encode_deviceptr,    // 源：Vulkan 导出的 CUDA 可访问指针
                static_cast<size_t>(m_extent.width * m_extent.height * 4),            // 拷贝大小（原始图像总字节数：original_extent.w * original_extent.h * 4）
                cudaMemcpyDeviceToDevice    // 拷贝类型：GPU 设备内存→GPU 设备内存
            );

            cudaUpsampleImage(
                (const uint8_t*)d_cuda_input,
                (uint8_t*)d_cuda_output,
                m_extent.width,
                m_extent.height,
                m_encode_extent.width,
                m_encode_extent.height
            );

            cudaDeviceSynchronize();
            encode_input = (void*)d_cuda_output;
        }
        CUcontext cuContext = cudaContext->get();
        NvEncoderCuda::CopyToDeviceFrame(cuContext,
            encode_input,
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

    // 解码一帧：H.264码流 -> NVDEC解码 -> YUV转BGRA -> 写入CUDA设备指针（Vulkan可零拷贝读取）
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