# CUDA-Vulkan Depth Interop（CUDA-Vulkan 深度互操作）

> 分析分支：`encoder`

---

## 1. 概述

CUDA-Vulkan Depth Interop 实现了 Vulkan 深度图像与 CUDA 之间的零拷贝内存共享，允许 CUDA 内核直接操作 Vulkan 分配的深度缓冲内存，无需 GPU-to-CPU 数据传输。

---

## 2. 位置

- **声明**：`asset/include/fixDepth.h` — `fix_depth_interop()`
- **实现**：外部 CUDA 文件（`fix_depth_interop` 内核）
- **互操作基础设施**：`asset/src/encoder.cpp` — `Cudaimage`, `Cudasema`
- **调度**：`asset/src/vsgRendererServer.cpp` — 渲染循环中调用

---

## 3. 互操作图像创建

### 3.1 深度互操作图像

在 `vsgRendererServer.cpp` 的 `initRenderer()` 中创建：

```cpp
// 创建深度互操作图像
interopDepthImage = vsg::Image::create();
interopDepthImage->imageType = VK_IMAGE_TYPE_2D;
interopDepthImage->format = VK_FORMAT_R16_UNORM;        // 16位无符号归一化
interopDepthImage->extent = {width, height, 1};
interopDepthImage->tiling = VK_IMAGE_TILING_LINEAR;     // LINEAR tiling 便于 CUDA 访问
interopDepthImage->usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                           VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
// 启用外部内存导出
VkExternalMemoryImageCreateInfo externalInfo = {};
externalInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
externalInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHR;
```

### 3.2 内存导出到 CUDA

通过 `getExportHandle()` 函数获取 Vulkan 内存的 Win32/FD 句柄，然后导入到 CUDA：

```cpp
// encoder.cpp
void* getExportHandle(vsg::ref_ptr<vsg::Image> image, vsg::ref_ptr<vsg::Device> m_device) {
    // vkGetMemoryWin32HandleKHR / vkGetMemoryFdKHR
    // 返回 HANDLE (Win32) 或 fd (Linux)
}

// Cudaimage 构造函数
CUDA_EXTERNAL_MEMORY_HANDLE_DESC memDesc = {};
memDesc.type = CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32;
memDesc.handle.win32.handle = handle;
memDesc.size = deviceSize;
cuImportExternalMemory(&m_extMem, &memDesc);

CUDA_EXTERNAL_MEMORY_BUFFER_DESC bufDesc = {};
bufDesc.size = memDesc.size;
cuExternalMemoryGetMappedBuffer(&m_deviceptr, m_extMem, &bufDesc);
```

---

## 4. 渲染循环中的调用

在每帧的渲染循环中（`vsgRendererServer.cpp`）：

```cpp
// 1. 修复深度互操作（CUDA 直接操作互操作内存）
fix_depth_interop(width, height, host_depth, depth_device_ptr);

// 2. 将互操作深度拷贝到 depthImage
copyInteropToDepthImage(commandBuffer, interopDepthImage, depthImage);
```

### 4.1 fix_depth_interop

```cpp
// fixDepth.h
void fix_depth_interop(int w, int h, unsigned short* host_depth, void* depth_device_ptr);
```

- 直接在 CUDA 上操作互操作深度内存
- 无需 D2H（Device-to-Host）传输
- 可能用于深度修复、滤波或其他 CUDA 处理

### 4.2 copyInteropToDepthImage

将互操作深度图像通过 Vulkan 命令缓冲区拷贝到 `depthImage`，供后续渲染使用。

---

## 5. 信号量同步

使用外部信号量在 Vulkan 和 CUDA 之间同步：

```cpp
// Cudasema 构造
CUDA_EXTERNAL_SEMAPHORE_HANDLE_DESC semDesc = {};
semDesc.type = CU_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32;
semDesc.handle.win32.handle = semaphoreHandle;
cuImportExternalSemaphore(&m_extSema, &semDesc);

// 等待/信号
CUresult Cudasema::wait(void) {
    return cuWaitExternalSemaphoresAsync(&m_extSema, &waitParams, 1, nullptr);
}
CUresult Cudasema::signal(void) {
    return cuSignalExternalSemaphoresAsync(&m_extSema, &signalParams, 1, nullptr);
}
```

---

## 6. 跨平台支持

| 平台 | 句柄类型 | API |
|------|---------|-----|
| Windows | `HANDLE` | `vkGetMemoryWin32HandleKHR` |
| Linux | `int fd` | `vkGetMemoryFdKHR` |

---

## 7. 依赖关系

- **前置依赖**：外部 CUDA 管线（提供深度数据）
- **后续依赖**：Main Render Pass（使用修复后的深度）
- **同步**：通过外部信号量确保 Vulkan 和 CUDA 的执行顺序
