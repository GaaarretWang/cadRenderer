#include <cuda_runtime.h>

// Bilinear upsampling kernel: input image (R8G8B8A8_UNORM) -> output image (R8G8B8A8_UNORM).
// inWidth/inHeight are the source resolution; outWidth/outHeight are the encoded resolution after upsampling.
__global__ static void upsampleBilinearKernel(
    const unsigned char* __restrict__ d_input,  // Input image (CUDA pointer for the original Vulkan image)
    unsigned char* __restrict__ d_output,       // Output image (CUDA pointer for encode_destination_image)
    int inWidth, int inHeight,
    int outWidth, int outHeight
) {
    // Compute the output pixel handled by the current thread.
    const int outX = blockIdx.x * blockDim.x + threadIdx.x;
    const int outY = blockIdx.y * blockDim.y + threadIdx.y;

    // Exit immediately when the thread falls outside the output image.
    if (outX >= outWidth || outY >= outHeight) return;

    // Map the output pixel to floating-point source coordinates for interpolation.
    const float scaleX = static_cast<float>(inWidth) / outWidth;
    const float scaleY = static_cast<float>(inHeight) / outHeight;
    const float inX = outX * scaleX;
    const float inY = outY * scaleY;

    // Gather the four neighboring source pixels needed for bilinear interpolation.
    const int x0 = static_cast<int>(floor(inX));
    const int y0 = static_cast<int>(floor(inY));
    const int x1 = min(x0 + 1, inWidth - 1);
    const int y1 = min(y0 + 1, inHeight - 1);

    // Interpolation weights in the [0, 1] range.
    const float wx = inX - x0;
    const float wy = inY - y0;

    // Compute memory indices for the four neighboring pixels (R8G8B8A8, four bytes per pixel).
    const int idx00 = (y0 * inWidth + x0) * 4;
    const int idx01 = (y0 * inWidth + x1) * 4;
    const int idx10 = (y1 * inWidth + x0) * 4;
    const int idx11 = (y1 * inWidth + x1) * 4;

    // Bilinearly interpolate each channel (R, G, B, A).
    for (int c = 0; c < 4; ++c) {
        const float val00 = d_input[idx00 + c];
        const float val01 = d_input[idx01 + c];
        const float val10 = d_input[idx10 + c];
        const float val11 = d_input[idx11 + c];

        // Bilinear formula: (1-wy)*[(1-wx)*val00 + wx*val01] + wy*[(1-wx)*val10 + wx*val11].
        const float val = (1.f - wy) * ((1.f - wx) * val00 + wx * val01) +
                          wy * ((1.f - wx) * val10 + wx * val11);

        // Write the interpolated value back as uint8_t.
        d_output[(outY * outWidth + outX) * 4 + c] = static_cast<unsigned char>(val);
    }
}

// Host-side entry point used by the C++ code.
void cudaUpsampleImage(
    const unsigned char* d_input,
    unsigned char* d_output,
    int inWidth, int inHeight,
    int outWidth, int outHeight
) {
    // Thread-block size (CUDA commonly uses 16x16 or 32x32).
    dim3 blockDim(32, 32);
    // Grid size rounded up to cover all output pixels.
    dim3 gridDim(
        (outWidth + blockDim.x - 1) / blockDim.x,
        (outHeight + blockDim.y - 1) / blockDim.y
    );

    // Launch the kernel.
    upsampleBilinearKernel<<<gridDim, blockDim>>>(
        d_input, d_output, inWidth, inHeight, outWidth, outHeight
    );
}
