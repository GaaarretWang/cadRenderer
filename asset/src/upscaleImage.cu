#include "fixDepth.h"
#include <cuda_runtime.h>

// 双线性插值上采样核函数：输入图像（R8G8B8A8_UNORM）→ 输出图像（R8G8B8A8_UNORM）
// inWidth/inHeight：原始分辨率；outWidth/outHeight：编码分辨率（上采样后）
__global__ static void upsampleBilinearKernel(
    const unsigned char* __restrict__ d_input,  // 输入图像（Vulkan 原始图像的 CUDA 指针）
    unsigned char* __restrict__ d_output,       // 输出图像（encode_destination_image 的 CUDA 指针）
    int inWidth, int inHeight,
    int outWidth, int outHeight
) {
    // 计算当前线程对应的输出像素坐标
    const int outX = blockIdx.x * blockDim.x + threadIdx.x;
    const int outY = blockIdx.y * blockDim.y + threadIdx.y;

    // 超出输出图像范围的线程直接返回
    if (outX >= outWidth || outY >= outHeight) return;

    // 计算输出像素在输入图像上的对应坐标（浮点数，用于插值）
    const float scaleX = static_cast<float>(inWidth) / outWidth;
    const float scaleY = static_cast<float>(inHeight) / outHeight;
    const float inX = outX * scaleX;
    const float inY = outY * scaleY;

    // 找到输入图像中插值所需的 4 个相邻像素（左上角、右上角、左下角、右下角）
    const int x0 = static_cast<int>(floor(inX));
    const int y0 = static_cast<int>(floor(inY));
    const int x1 = min(x0 + 1, inWidth - 1);
    const int y1 = min(y0 + 1, inHeight - 1);

    // 插值权重（0~1）
    const float wx = inX - x0;
    const float wy = inY - y0;

    // 计算 4 个相邻像素的内存索引（R8G8B8A8，每个像素 4 字节）
    const int idx00 = (y0 * inWidth + x0) * 4;
    const int idx01 = (y0 * inWidth + x1) * 4;
    const int idx10 = (y1 * inWidth + x0) * 4;
    const int idx11 = (y1 * inWidth + x1) * 4;

    // 双线性插值计算每个通道（R、G、B、A）
    for (int c = 0; c < 4; ++c) {
        const float val00 = d_input[idx00 + c];
        const float val01 = d_input[idx01 + c];
        const float val10 = d_input[idx10 + c];
        const float val11 = d_input[idx11 + c];

        // 双线性插值公式：(1-wy)*[(1-wx)*val00 + wx*val01] + wy*[(1-wx)*val10 + wx*val11]
        const float val = (1.f - wy) * ((1.f - wx) * val00 + wx * val01) +
                          wy * ((1.f - wx) * val10 + wx * val11);

        // 写入输出图像（转换为 uint8_t）
        d_output[(outY * outWidth + outX) * 4 + c] = static_cast<unsigned char>(val);
    }
}

// 宿主端调用接口（供 C++ 代码调用）
void cudaUpsampleImage(
    const unsigned char* d_input,
    unsigned char* d_output,
    int inWidth, int inHeight,
    int outWidth, int outHeight
) {
    // 线程块大小（CUDA 推荐 16x16 或 32x32）
    dim3 blockDim(32, 32);
    // 线程网格大小（向上取整覆盖所有输出像素）
    dim3 gridDim(
        (outWidth + blockDim.x - 1) / blockDim.x,
        (outHeight + blockDim.y - 1) / blockDim.y
    );

    // 启动核函数
    upsampleBilinearKernel<<<gridDim, blockDim>>>(
        d_input, d_output, inWidth, inHeight, outWidth, outHeight
    );
}