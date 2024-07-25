#include "encoder.h"
#include <array>

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

CUresult Cudactx::memcpyDtoH(void* p, CUdeviceptr dptr, size_t size)
{
   return cuMemcpyDtoH(p, dptr, size);
}

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
        vkGetDeviceProcAddr(m_device->vk(), "vkGetMemoryFdKHR");
    if (!func ||
        func(m_device->vk(), &fdInfo, &fd) != VK_SUCCESS) {
        return nullptr;
    }
    return (void *)(uintptr_t)fd;
#endif

}

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

Cudaimage::Cudaimage(vsg::ref_ptr<vsg::Image> image, vsg::ref_ptr<vsg::Device> m_device, VkDeviceSize deviceSize, VkExtent2D extent)
{
    void* p = nullptr;
    CUresult result = CUDA_SUCCESS;

    if ((p = getExportHandle(image, m_device)) == nullptr)
    {
        throw std::runtime_error("Failed to get export handle for memory");
    }

    CUDA_EXTERNAL_MEMORY_HANDLE_DESC memDesc = {};
#ifndef _WIN32
    memDesc.type = CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD;
#else
    memDesc.type = CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32;
#endif
    memDesc.handle.win32.handle = p;
    memDesc.size = deviceSize;

    if (cuImportExternalMemory(&m_extMem, &memDesc) != CUDA_SUCCESS)
    {
        throw std::runtime_error("Failed to import buffer into CUDA");
    }

    CUDA_ARRAY3D_DESCRIPTOR arrayDesc = {};
    arrayDesc.Width = extent.width;
    arrayDesc.Height = extent.height;
    arrayDesc.Depth = 0; /* CUDA 2D arrays are defined to have depth 0 */
    arrayDesc.Format = CU_AD_FORMAT_UNSIGNED_INT8;
    arrayDesc.NumChannels = 4;
    arrayDesc.Flags = CUDA_ARRAY3D_SURFACE_LDST |
                      CUDA_ARRAY3D_COLOR_ATTACHMENT;

    CUDA_EXTERNAL_MEMORY_MIPMAPPED_ARRAY_DESC mipmapArrayDesc = {};
    mipmapArrayDesc.arrayDesc = arrayDesc;
    mipmapArrayDesc.numLevels = 1;

    result = cuExternalMemoryGetMappedMipmappedArray(&m_mipmapArray, m_extMem,
                                                     &mipmapArrayDesc);
    if (result != CUDA_SUCCESS)
    {
        std::ostringstream oss;
        oss << "Failed to get CUmipmappedArray; " << result;
        throw std::runtime_error(oss.str());
    }

    result = cuMipmappedArrayGetLevel(&m_array, m_mipmapArray, 0);
    if (result != CUDA_SUCCESS)
    {
        std::ostringstream oss;
        oss << "Failed to get CUarray; " << result;
        throw std::runtime_error(oss.str());
    }
}

Cudaimage::~Cudaimage()
{
    cuMipmappedArrayDestroy(m_mipmapArray);
    cuDestroyExternalMemory(m_extMem);
    m_array = 0;
    m_mipmapArray = 0;
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

Cudasema::~Cudasema()
{
    cuDestroyExternalSemaphore(m_extSema);
}

CUresult Cudasema::wait(void)
{
    CUDA_EXTERNAL_SEMAPHORE_WAIT_PARAMS waitParams = {};

    return cuWaitExternalSemaphoresAsync(&m_extSema, &waitParams, 1, nullptr);
}

CUresult Cudasema::signal(void)
{
    CUDA_EXTERNAL_SEMAPHORE_SIGNAL_PARAMS signalParams = {};

    return cuSignalExternalSemaphoresAsync(&m_extSema, &signalParams, 1, nullptr);
}
