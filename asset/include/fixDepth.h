// CUDA-Vulkan interop path: operate directly on interop memory without any D2H copy.
// depth_ptr: CUDA device pointer that targets the Vulkan interop depth-image memory.
void fix_depth_interop(int w, int h, unsigned short * host_depth, void* depth_device_ptr);
