#include "encoder.h"
#include <array>

/**
 * 获取Vulkan物理设备的UUID
 *
 * 为什么需要UUID：Vulkan和CUDA是两套独立的API，各自维护设备列表。
 * 在多GPU系统中，Vulkan的device 0不一定对应CUDA的device 0。
 * UUID是GPU的唯一标识符，是跨API匹配设备的唯一可靠方式。
 *
 * 使用的Vulkan扩展：VK_KHR_get_physical_device_properties2
 * 返回的UUID格式与CUDA的CUuuid完全一致（16字节）
 */
void NvEncoderWrapper::getDeviceUUID(
    vsg::Instance* instance, vsg::ref_ptr<vsg::Device> mDevice,
    std::array<uint8_t, VK_UUID_SIZE>& deviceUUID
)
{
    VkPhysicalDeviceIDPropertiesKHR deviceIDProps = {};
    deviceIDProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES_KHR;

    VkPhysicalDeviceProperties2KHR props = {};
    props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
    props.pNext = &deviceIDProps;

    auto func = (PFN_vkGetPhysicalDeviceProperties2KHR) \
        vkGetInstanceProcAddr(instance->vk(), "vkGetPhysicalDeviceProperties2KHR");
    if (func == nullptr) {
        throw std::runtime_error("Failed to load vkGetPhysicalDeviceProperties2KHR");
    }

    func(mDevice->getPhysicalDevice()->vk(), &props);

    std::memcpy(deviceUUID.data(), deviceIDProps.deviceUUID, VK_UUID_SIZE);
}

/**
 * CUDA上下文构造函数 —— 通过UUID匹配找到与Vulkan相同的GPU
 *
 * 流程：
 *   1. cuInit(0) 初始化CUDA驱动API
 *   2. cuDeviceGetCount 获取系统中CUDA设备数量
 *   3. 遍历所有CUDA设备，用cuDeviceGetUuid获取每个设备的UUID
 *   4. 与传入的Vulkan设备UUID逐字节比较，找到匹配的设备
 *   5. 在匹配的设备上创建CUDA context
 *
 * 注意：这里用的是CUDA驱动API（cu前缀），不是运行时API（cuda前缀）
 *      驱动API更底层，支持直接操作CUcontext，与NVENC配合使用
 */
Cudactx::Cudactx(std::array<uint8_t, VK_UUID_SIZE>& deviceUUID)
{
   CUdevice dev;
   CUresult result = CUDA_SUCCESS;
   bool foundDevice = true;

   result = cuInit(0);
   if (result != CUDA_SUCCESS) {
       throw std::runtime_error("Failed to cuInit()");
   }

   int numDevices = 0;
   result = cuDeviceGetCount(&numDevices);
   if (result != CUDA_SUCCESS) {
       throw std::runtime_error("Failed to get count of CUDA devices");
   }

   CUuuid id = {};

   /*
    * Loop over the available devices and identify the CUdevice
    * corresponding to the physical device in use by this Vulkan instance.
    * This is required because there is no other way to match GPUs across
    * API boundaries.
    */
   for (int i = 0; i < numDevices; i++) {
       cuDeviceGet(&dev, i);

       cuDeviceGetUuid(&id, dev);

       if (!std::memcmp(static_cast<const void*>(&id),
           static_cast<const void*>(deviceUUID.data()),
           sizeof(CUuuid))) {
           foundDevice = true;
           break;
       }
   }

   if (!foundDevice) {
       throw std::runtime_error("Failed to get an appropriate CUDA device");
   }

   result = cuCtxCreate(&m_context, 0, dev);
   if (result != CUDA_SUCCESS) {
       throw std::runtime_error("Failed to create a CUDA context");
   }
}

// 一维设备到主机拷贝：将CUdeviceptr指向的GPU显存数据拷贝到CPU内存
// p: CPU侧目标缓冲区, dptr: GPU侧源地址, size: 拷贝字节数
CUresult Cudactx::memcpyDtoH(void* p, CUdeviceptr dptr, size_t size)
{
   return cuMemcpyDtoH(p, dptr, size);
}

// 二维设备到主机拷贝：从CUarray（CUDA数组）拷贝到CPU内存
// 与memcpyDtoH的区别：支持2D拷贝和pitch对齐，常用于图像数据传输
// p: CPU侧目标缓冲区, array: CUDA数组源, width: 每行字节数, height: 行数
CUresult Cudactx::memcpy2D(
   void* p, CUarray array, uint32_t width, uint32_t height
)
{
   CUDA_MEMCPY2D copy = {};
   copy.srcMemoryType = CU_MEMORYTYPE_ARRAY;
   copy.srcArray = array;
   copy.dstMemoryType = CU_MEMORYTYPE_HOST;
   copy.dstHost = p;
   copy.dstPitch = width;
   copy.WidthInBytes = width;
   copy.Height = height;

   return cuMemcpy2D(&copy);
}

/**
 * Vulkan图像内存屏障构造函数
 *
 * 什么是Image Memory Barrier：它是Vulkan同步机制的一种，用于：
 *   1. 改变Image的layout（例如从COLOR_ATTACHMENT_OPTIMAL变为TRANSFER_SRC_OPTIMAL）
 *   2. 确保之前的写入对后续的读取可见（memory dependency）
 *
 * 这里只设置基本参数（image句柄、子资源范围），具体的行为控制（src/dstAccessMask、
 * old/newLayout）在外部使用时再配置。
 *
 * VK_QUEUE_FAMILY_IGNORED：表示不进行队列族所有权转移（单队列使用场景）
 */
Vkimgmembarrier::Vkimgmembarrier(vsg::ref_ptr<vsg::Image> image, uint32_t deviceID)
{
    m_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    m_barrier.pNext = nullptr;
    m_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    m_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    m_barrier.image = image->vk(deviceID);
    m_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    m_barrier.subresourceRange.baseMipLevel = 0;
    m_barrier.subresourceRange.levelCount = 1;
    m_barrier.subresourceRange.baseArrayLayer = 0;
    m_barrier.subresourceRange.layerCount = 1;
}

/**
 * 从Vulkan Image的显存导出跨API共享句柄
 *
 * 这是Vulkan-CUDA互操作的第一步：获取显存的"外部句柄"
 *   - Windows：返回HANDLE（Win32内核句柄）
 *   - Linux：返回fd（文件描述符）
 *
 * 前提条件：Vulkan Image必须使用VkExternalMemoryImageCreateInfo创建，
 *          并分配了VkExportMemoryAllocateInfo标记的显存。
 *
 * 原理：Vulkan显存本质上是GPU驱动管理的一块内存区域。
 *      通过导出句柄，CUDA驱动可以通过这个"指针"访问同一块显存，
 *      从而实现两个API之间的零拷贝数据共享。
 */
void* getExportHandle(vsg::ref_ptr<vsg::Image> image, vsg::ref_ptr<vsg::Device> m_device)
{
#ifdef _WIN32
    HANDLE handle = INVALID_HANDLE_VALUE; 
    VkMemoryGetWin32HandleInfoKHR handleInfo = {};
    handleInfo.sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR;
    handleInfo.memory = image->getDeviceMemory(m_device->deviceID)->vk();
    handleInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHR;

    auto func = (PFN_vkGetMemoryWin32HandleKHR) \
        vkGetDeviceProcAddr(m_device->vk(), "vkGetMemoryWin32HandleKHR");
    if (!func) {
        std::cout << "Failed to get vkGetMemoryWin32HandleKHR function" << std::endl;
        return nullptr;
    }
    VkResult result = func(m_device->vk(), &handleInfo, &handle);
    if (result != VK_SUCCESS)
    {
        std::cout << "Failed to get Win32 handle" << std::endl;
        return nullptr;
    }
    return handle;
#else
    int fd = -1;
    VkMemoryGetFdInfoKHR fdInfo = {};
    fdInfo.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
    fdInfo.memory = image->getDeviceMemory(m_device->deviceID)->vk();
    fdInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;

    auto func = (PFN_vkGetMemoryFdKHR) \
        vkGetDeviceProcAddr(m_device->vk(), "vkGetMemoryFdKHR");//

    //VkResult a =func(m_device->vk(), &fdInfo, &fd);

    if (!func ||
        func(m_device->vk(), &fdInfo, &fd) != VK_SUCCESS) {
        return nullptr;
    }
    return (void *)(uintptr_t)fd;
#endif

}

/**
 * 从Vulkan Buffer的显存导出跨API共享句柄（Buffer版本）
 * 与Image版本原理相同，只是操作对象是VkBuffer而非VkImage
 * 当前代码中未被使用（被注释掉的readBackVulkanBuffer方法会用到）
 */
void* getExportHandle(vsg::ref_ptr<vsg::Buffer> buffer, vsg::ref_ptr<vsg::Device> m_device)
{

#ifdef _WIN32
    HANDLE handle = INVALID_HANDLE_VALUE; // ʹ�� HANDLE �����������洢 Win32 ���
    VkMemoryGetWin32HandleInfoKHR handleInfo = {};
    handleInfo.sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR;
    handleInfo.memory = buffer->getDeviceMemory(m_device->deviceID)->vk();
    handleInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHR;

    auto func = (PFN_vkGetMemoryWin32HandleKHR) \
        vkGetDeviceProcAddr(m_device->vk(), "vkGetMemoryWin32HandleKHR");
    if (!func) {
        std::cout << "Failed to get vkGetMemoryWin32HandleKHR function" << std::endl;
        return nullptr;
    }
    VkResult result = func(m_device->vk(), &handleInfo, &handle);
    if (result != VK_SUCCESS) {
        std::cout << "Failed to get Win32 handle" << std::endl;
        return nullptr;
    }
    return handle;
#else
    int fd = -1;

    VkMemoryGetFdInfoKHR fdInfo = {};
    fdInfo.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
    fdInfo.memory = buffer->getDeviceMemory(m_device->deviceID)->vk();
    fdInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;

    auto func = (PFN_vkGetMemoryFdKHR) \
        vkGetDeviceProcAddr(m_device->vk(), "vkGetMemoryFdKHR");

    if (!func ||
        func(m_device->vk(), &fdInfo, &fd) != VK_SUCCESS) {
        return nullptr;
    }

    return (void *)(uintptr_t)fd;
#endif
}

/**
 * Cudaimage构造函数 —— 将Vulkan Image的显存导入CUDA，实现零拷贝访问
 *
 * 详细流程：
 *   1. getExportHandle(): 获取Vulkan显存的Win32 HANDLE或Linux fd
 *   2. cuImportExternalMemory(): 通过句柄将Vulkan显存导入CUDA，得到CUexternalMemory
 *      - CUDA驱动通过这个句柄直接映射Vulkan的显存地址空间
 *   3. cuExternalMemoryGetMappedBuffer(): 将外部显存映射为CUdeviceptr
 *      - 得到的CUdeviceptr可以直接用于cudaMemcpy、CUDA kernel、或NVENC输入
 *
 * 关键理解：
 *   - 没有数据拷贝发生，CUDA和Vulkan共享同一块物理显存
 *   - CUdeviceptr本质上是一个GPU虚拟地址指针
 *   - 必须配合Cudasema（信号量）使用，确保读写时序正确
 */
Cudaimage::Cudaimage(vsg::ref_ptr<vsg::Image> image, vsg::ref_ptr<vsg::Device> m_device, VkDeviceSize deviceSize, VkExtent2D extent)
{
    void* p = nullptr;
    CUresult result = CUDA_SUCCESS;

    if ((p = getExportHandle(image, m_device)) == nullptr)
    {
        throw std::runtime_error("Failed to get export handle for memory");
    }

//     CUDA_EXTERNAL_MEMORY_HANDLE_DESC memDesc = {};
// #ifndef _WIN32
//     memDesc.type = CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD;
// #else
//     memDesc.type = CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32;
// #endif
//     memDesc.handle.win32.handle = p;
//     memDesc.size = deviceSize;

//     if (cuImportExternalMemory(&m_extMem, &memDesc) != CUDA_SUCCESS)
//     {
//         throw std::runtime_error("Failed to import buffer into CUDA");
//     }

//     CUDA_ARRAY3D_DESCRIPTOR arrayDesc = {};
//     arrayDesc.Width = extent.width;
//     arrayDesc.Height = extent.height;
//     arrayDesc.Depth = 0; /* CUDA 2D arrays are defined to have depth 0 */
//     arrayDesc.Format = CU_AD_FORMAT_UNSIGNED_INT8;
//     arrayDesc.NumChannels = 4;
//     arrayDesc.Flags = CUDA_ARRAY3D_SURFACE_LDST |
//                       CUDA_ARRAY3D_COLOR_ATTACHMENT;

//     CUDA_EXTERNAL_MEMORY_MIPMAPPED_ARRAY_DESC mipmapArrayDesc = {};
//     mipmapArrayDesc.arrayDesc = arrayDesc;
//     mipmapArrayDesc.numLevels = 1;

//     result = cuExternalMemoryGetMappedMipmappedArray(&m_mipmapArray, m_extMem,
//                                                      &mipmapArrayDesc);
//     if (result != CUDA_SUCCESS)
//     {
//         std::ostringstream oss;
//         oss << "Failed to get CUmipmappedArray; " << result;
//         throw std::runtime_error(oss.str());
//     }

//     result = cuMipmappedArrayGetLevel(&m_array, m_mipmapArray, 0);
//     if (result != CUDA_SUCCESS)
//     {
//         std::ostringstream oss;
//         oss << "Failed to get CUarray; " << result;
//         throw std::runtime_error(oss.str());
//     }

    CUDA_EXTERNAL_MEMORY_HANDLE_DESC memDesc = {};
    #ifdef _WIN32
    memDesc.type = CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32;
    #else
    memDesc.type = CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD;
    #endif
    memDesc.handle.fd = (int)(uintptr_t)p;
    memDesc.size = deviceSize;

    if (cuImportExternalMemory(&m_extMem, &memDesc) != CUDA_SUCCESS) {
        throw std::runtime_error("Failed to import buffer into CUDA");
    }

    CUDA_EXTERNAL_MEMORY_BUFFER_DESC bufDesc = {};
    bufDesc.size = memDesc.size;

    if (cuExternalMemoryGetMappedBuffer(&m_deviceptr, m_extMem, &bufDesc) !=
        CUDA_SUCCESS) {
        throw std::runtime_error("Failed to get CUdeviceptr");
    }

}

// 析构：释放CUDA侧导入的外部显存
// 注意：这不会释放Vulkan侧的Image，Vulkan有自己的生命周期管理
Cudaimage::~Cudaimage()
{
    // cuMipmappedArrayDestroy(m_mipmapArray);
    cuDestroyExternalMemory(m_extMem);
    // m_array = 0;
    // m_mipmapArray = 0;
}

//MFW::Wrapper::Buffer::Ptr NvEncoderWrapper::readBackVulkanBuffer(CUdeviceptr dpFrame) {
//    cudaExternalMemory_t m_extMem;
//    //MFW::Wrapper::Image::Ptr vulkanImage = MFW::Wrapper::Image::create(device);
//    //VkExternalMemoryImageCreateInfo externalMemoryImageCreateInfo = {};
//    //externalMemoryImageCreateInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
//    //externalMemoryImageCreateInfo.pNext = nullptr;
//    //externalMemoryImageCreateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
//
//    //VkImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
//    //imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
//    //imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
//    //imageCreateInfo.format = VK_FORMAT_B8G8R8A8_SRGB;
//    //imageCreateInfo.extent.width = mWidth;
//    //imageCreateInfo.extent.height = mHeight;
//    //imageCreateInfo.extent.depth = 1;
//    //imageCreateInfo.mipLevels = 1;
//    //imageCreateInfo.arrayLayers = 1;
//    //imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
//    //imageCreateInfo.tiling = VK_IMAGE_TILING_LINEAR;
//    //imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
//    //imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
//    //imageCreateInfo.pNext = &externalMemoryImageCreateInfo;
//
//    //vulkanImage->createExtensionImage(imageCreateInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
//
//    m_readBackBuffer = MFW::Wrapper::Buffer::create(device, 4 * mWidth * mHeight, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
//        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, true);
//
//    void* p = nullptr;
//    CUresult result = CUDA_SUCCESS;
//
//    if ((p = getExportHandle(m_readBackBuffer, device)) == nullptr) {
//        throw std::runtime_error("Failed to get export handle for memory");
//    }
//
////    CUDA_EXTERNAL_MEMORY_HANDLE_DESC memDesc = {};
////#ifndef _WIN32
////    memDesc.type = CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD;
////#else
////    memDesc.type = CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32;
////#endif
////    memDesc.handle.win32.handle = p;
////    memDesc.size = 4 * mWidth * mHeight;
////    if (cuImportExternalMemory(&m_extMem, &memDesc) != CUDA_SUCCESS) {
////        throw std::runtime_error("Failed to import buffer into CUDA");
////    }
//
//
//    cudaExternalMemoryHandleDesc externalMemoryHandleDesc;
//    memset(&externalMemoryHandleDesc, 0, sizeof(externalMemoryHandleDesc));
//
//    externalMemoryHandleDesc.type = cudaExternalMemoryHandleTypeOpaqueWin32;
//    externalMemoryHandleDesc.handle.win32.handle = p;
//    externalMemoryHandleDesc.size = 4 * mWidth * mHeight;
//    if (cudaImportExternalMemory(&m_extMem, &externalMemoryHandleDesc) != CUDA_SUCCESS) {
//        throw std::runtime_error("Failed to import buffer into CUDA");
//    }
//
//    CUdeviceptr* m_cudaDevVertptr;
//
//    cudaExternalMemoryBufferDesc externalMemoryBufferDesc;
//    memset(&externalMemoryBufferDesc, 0, sizeof(externalMemoryBufferDesc));
//    externalMemoryBufferDesc.offset = 0;
//    externalMemoryBufferDesc.size = 4 * mWidth * mHeight;
//    externalMemoryBufferDesc.flags = 0;
//    cudaError_t result1 = cudaExternalMemoryGetMappedBuffer((void**)&m_cudaDevVertptr, m_extMem, &externalMemoryBufferDesc);
//    if (result1 != CUDA_SUCCESS) {
//        //const char* errorString;
//        //cuGetErrorName(result1, &errorString);
//        //printf("CUDA Error: %s\n", errorString);
//
//        throw std::runtime_error("Failed to output CUDA into buffer into");
//    }
//
//    CUDA_MEMCPY2D copy = {};
//    copy.srcMemoryType = CU_MEMORYTYPE_DEVICE;
//    copy.srcDevice = dpFrame;
//    copy.dstMemoryType = CU_MEMORYTYPE_DEVICE;
//    copy.dstDevice = (CUdeviceptr)m_cudaDevVertptr;
//    copy.WidthInBytes = 4 * mWidth;
//    copy.Height = mHeight;
//
//    result = cuMemcpy2D(&copy);
//    if (result != CUDA_SUCCESS) {
//        const char* errorString;
//        cuGetErrorString(result, &errorString);
//        printf("CUDA Error: %s\n", errorString);
//
//        throw std::runtime_error("Failed cuMemcpy2D");
//    }
//    cudaDestroyExternalMemory(m_extMem);
//    return m_readBackBuffer;
//
//    //CUDA_EXTERNAL_MEMORY_BUFFER_DESC bufferDesc;
//    //bufferDesc.offset = 0;
//    //bufferDesc.flags = 0;
//    //bufferDesc.size = 4 * mWidth * mHeight;
//    //CUdeviceptr mapCUdeviceptr;
//    //result = cudaExternalMemoryGetMappedBuffer(&mapCUdeviceptr, &m_extMem, &bufferDesc);
//    //if (result != CUDA_SUCCESS) {
//    //    const char* errorString;
//    //    cuGetErrorName(result, &errorString);
//    //    printf("CUDA Error: %s\n", errorString);
//
//    //    //throw std::runtime_error("Failed to output CUDA into buffer into");
//    //}
//    //else {
//    //    std::cout << "yessssssssss";
//    //}
//
//    //CUDA_ARRAY_DESCRIPTOR arrayDesc;
//    //memset(&arrayDesc, 0, sizeof(arrayDesc));
//    //arrayDesc.Format = CU_AD_FORMAT_UNSIGNED_INT8;
//    //arrayDesc.NumChannels = 4;
//    //arrayDesc.Width = mWidth;
//    //arrayDesc.Height = mHeight;
//
//    //CUarray cuArray;
//    //CUresult result = cuArrayCreate(&cuArray, &arrayDesc);
//    //if (result != CUDA_SUCCESS) {
//    //    throw std::runtime_error("Failed cuArrayCreate");
//    //}
//
//    //CUDA_MEMCPY2D copy = {};
//    //copy.srcMemoryType = CU_MEMORYTYPE_DEVICE;
//    //copy.srcDevice = dpFrame;
//    //copy.dstMemoryType = CU_MEMORYTYPE_ARRAY;
//    //copy.dstArray = cuArray;
//    //copy.WidthInBytes = 4 * mWidth;
//    //copy.Height = mHeight;
//
//    //result = cuMemcpy2D(&copy);
//    //if (result != CUDA_SUCCESS) {
//    //    const char* errorString;
//    //    cuGetErrorString(result, &errorString);
//    //    printf("CUDA Error: %s\n", errorString);
//
//    //    throw std::runtime_error("Failed cuMemcpy2D");
//    //}
//
//    //void* hostData = malloc(mWidth * mHeight * 4);
//    //copy.srcMemoryType = CU_MEMORYTYPE_ARRAY;
//    //copy.srcArray = cuArray;
//    //copy.dstMemoryType = CU_MEMORYTYPE_HOST;
//    //copy.dstHost = hostData;
//    //copy.WidthInBytes = 4 * mWidth;
//    //copy.Height = mHeight;
//
//    //result = cuMemcpy2D(&copy);
//    //if (result != CUDA_SUCCESS) {
//    //    const char* errorString;
//    //    cuGetErrorString(result, &errorString);
//    //    printf("CUDA Error: %s\n", errorString);
//
//    //    throw std::runtime_error("Failed cuMemcpy2D");
//    //}
//    //fpOut.write(reinterpret_cast<char*>(hostData), mWidth * mHeight * 4);
//}
//
//

/**
 * 从Vulkan Semaphore导出跨API共享句柄
 *
 * 与getExportHandle(Image)原理类似，但导出的是信号量而非显存。
 *   - Windows：返回HANDLE，被转为CUexternalSemaphore
 *   - Linux：返回fd，被转为CUexternalSemaphore
 *
 * 信号量的作用：Vulkan和CUDA各自有自己的命令队列，信号量是两个队列之间的"交通灯"：
 *   - Vulkan画完一帧后对信号量做signal -> CUDA的wait返回，开始处理
 *   - CUDA处理完后对信号量做signal -> Vulkan的wait返回，继续渲染下一帧
 */
CUexternalSemaphore getExportHandle(vsg::ref_ptr<vsg::Semaphore> semaphore, vsg::ref_ptr<vsg::Device> m_device)
{
#ifdef _WIN32
    HANDLE handle = nullptr;

    VkSemaphoreGetWin32HandleInfoKHR semaHandleInfo = {};
    semaHandleInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR;
    semaHandleInfo.semaphore = semaphore->vk();
    semaHandleInfo.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHR;

    auto func = (PFN_vkGetSemaphoreWin32HandleKHR) \
        vkGetDeviceProcAddr(m_device->vk(), "vkGetSemaphoreWin32HandleKHR");
    if (!func ||
        func(m_device->vk(), &semaHandleInfo, &handle) != VK_SUCCESS) {
        return nullptr;
    }
    intptr_t handleValue = reinterpret_cast<intptr_t>(handle);
    CUexternalSemaphore extSemaHandle = (CUexternalSemaphore)handleValue;

    return extSemaHandle;
#else
    int fd = -1;

    VkSemaphoreGetFdInfoKHR fdInfo = {};
    fdInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR;
    fdInfo.semaphore = semaphore->vk();
    fdInfo.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;

    auto func = (PFN_vkGetSemaphoreFdKHR) \
        vkGetDeviceProcAddr(m_device->vk(), "vkGetSemaphoreFdKHR");

    if (!func ||
        func(m_device->vk(), &fdInfo, &fd) != VK_SUCCESS) {
        return nullptr;
    }
    CUexternalSemaphore extSemaHandle = (CUexternalSemaphore)fd;

    return extSemaHandle;
#endif
}

/**
 * Cudasema构造函数 —— 将Vulkan信号量导入CUDA
 *
 * 流程：
 *   1. getExportHandle(): 获取Vulkan Semaphore的Win32 HANDLE或Linux fd
 *   2. cuImportExternalSemaphore(): 将句柄导入为CUexternalSemaphore
 *
 * 导入后，CUDA可以通过cuWaitExternalSemaphoresAsync/cuSignalExternalSemaphoresAsync
 * 与Vulkan进行异步同步（不阻塞CPU，只阻塞GPU命令流）。
 */
Cudasema::Cudasema(vsg::ref_ptr<vsg::Semaphore> semaphore, vsg::ref_ptr<vsg::Device> m_device)
{
#ifdef _WIN32
    HANDLE handle = nullptr;
    CUexternalSemaphore p = NULL;

    // 获取信号量的句柄，此处假设 getExportHandle 函数用于获取信号量句柄
    if ((p = getExportHandle(semaphore, m_device)) == NULL) {
        throw std::runtime_error("Failed to get export handle for semaphore");
    }

    CUDA_EXTERNAL_SEMAPHORE_HANDLE_DESC semDesc = {};
    semDesc.type = CU_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32;
    semDesc.handle.win32.handle = p;
#else
    int fd = -1;
    CUexternalSemaphore p = NULL;

    // 获取信号量的句柄，此处假设 getExportHandle 函数用于获取信号量句柄
    if ((p = getExportHandle(semaphore, m_device)) == NULL) {
        throw std::runtime_error("Failed to get export handle for semaphore");
    }

    CUDA_EXTERNAL_SEMAPHORE_HANDLE_DESC semDesc = {};
    semDesc.type = CU_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD;
    semDesc.handle.fd = (int)(uintptr_t)p;
#endif

    if (cuImportExternalSemaphore(&m_extSema, &semDesc) != CUDA_SUCCESS) {
        throw std::runtime_error("Failed to import semaphore into CUDA");
    }
}

// 析构：销毁CUDA侧导入的外部信号量
Cudasema::~Cudasema()
{
    cuDestroyExternalSemaphore(m_extSema);
}

// CUDA侧等待Vulkan信号量（异步操作，不阻塞CPU）
// 阻塞CUDA命令流直到Vulkan侧对该信号量执行signal
// 典型场景：CUDA等待Vulkan渲染完成后再读取Image数据
CUresult Cudasema::wait(void)
{
    CUDA_EXTERNAL_SEMAPHORE_WAIT_PARAMS waitParams = {};

    return cuWaitExternalSemaphoresAsync(&m_extSema, &waitParams, 1, nullptr);
}

// CUDA侧通知Vulkan信号量（异步操作，不阻塞CPU）
// 告诉Vulkan侧CUDA处理已完成，Vulkan可以安全地使用该Image
// 典型场景：CUDA完成深度修正后，通知Vulkan可以进行后续渲染
CUresult Cudasema::signal(void)
{
    CUDA_EXTERNAL_SEMAPHORE_SIGNAL_PARAMS signalParams = {};

    return cuSignalExternalSemaphoresAsync(&m_extSema, &signalParams, 1, nullptr);
}
