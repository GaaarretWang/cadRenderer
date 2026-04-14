/**
 * CUDA深度修正函数（Vulkan互操作版本）
 *
 * 功能：在GPU上修正深度图数据（例如修复Z-fighting、反转深度、归一化等处理）
 *
 * 与传统方案的区别：
 *   - 传统方案：Vulkan Image -> GPU回传CPU(D2H) -> CPU处理 -> CPU上传GPU(H2D) -> Vulkan Image
 *     缺点：两次PCIe传输，带宽瓶颈，延迟高
 *   - 互操作方案：Vulkan Image显存被CUDA直接映射 -> CUDA kernel原地处理 -> 无回传
 *     优点：零CPU参与，零拷贝，直接在显存上操作
 *
 * 参数说明：
 *   @param w              图像宽度（像素）
 *   @param h              图像高度（像素）
 *   @param host_depth     CPU侧深度缓冲区（unsigned short数组，可能用于辅助数据或结果验证）
 *   @param depth_device_ptr  CUDA设备指针（CUdeviceptr），指向Vulkan互操作深度图像的显存
 *                            这个指针来自Cudaimage::get()，本质上是Vulkan Image的显存地址
 */
void fix_depth_interop(int w, int h, unsigned short * host_depth, void* depth_device_ptr);
