#include "fixDepth.h"
#include <cuda_runtime.h>
__global__ static void convert_4_to_channels(int w, int h, unsigned short * depth_pixels_cuda) {  
    int idx = blockIdx.x * blockDim.x + threadIdx.x;  
    int idy = blockIdx.y * blockDim.y + threadIdx.y;  

    if (idx < w && idy < h) {  
        unsigned short cur_depth_color = depth_pixels_cuda[w * idy + idx];
        if(cur_depth_color > 100)
            return;
        else{
            unsigned short count = 0;
            unsigned short total = 0;
            for(int m = -1; m <= 1; m ++){ //height
                for(int n = -1; n <= 1; n ++){ //width
                    if((idy + m) >= 0 && (idy + m) < h && (idx + n) >= 0 && (idx + n) < w){
                        unsigned short neighbor_pixel = depth_pixels_cuda[w * (idy + m) + idx + n];
                        
                        if(neighbor_pixel > 100){
                            total += neighbor_pixel;
                            count ++;
                        }
                    }
                }
            }
            if(count > 0)
                depth_pixels_cuda[w * idy + idx] = total / count;
            return;
        }

    }  
}  

// CUDA-Vulkan interop path: upload CPU depth into interop memory and run in-place inpainting with no D2H copy.
void fix_depth_interop(int w, int h, unsigned short * host_depth, void* depth_device_ptr){
    unsigned short* interop_ptr = static_cast<unsigned short*>(depth_device_ptr);

    // CPU -> interop GPU memory (write directly into Vulkan-sampled memory).
    cudaMemcpy(interop_ptr, host_depth, sizeof(unsigned short) * w * h, cudaMemcpyHostToDevice);

    dim3 block(16, 16);
    dim3 grid((w + block.x - 1) / block.x,
              (h + block.y - 1) / block.y);

    // Run the inpainting kernel in place on the interop memory.
    for(int iterate_num = 0; iterate_num < 15; iterate_num++){
        convert_4_to_channels<<<grid, block>>>(w, h, interop_ptr);
    }
    cudaDeviceSynchronize();
    // No cudaMemcpyDeviceToHost is needed because Vulkan samples this memory directly.
}
