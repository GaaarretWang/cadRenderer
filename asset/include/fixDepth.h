// CUDA-Vulkan interop版本：直接在interop内存上操作，无需D2H回传
// depth_ptr: CUDA device pointer 指向 Vulkan interop 深度图像内存
void fix_depth_interop(int w, int h, unsigned short * host_depth, void* depth_device_ptr);
