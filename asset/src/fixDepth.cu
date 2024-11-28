#include "fixDepth.h"

__global__ static void convert_4_to_channels(int w, int h) {  
    int idx = blockIdx.x * blockDim.x + threadIdx.x;  
    int idy = blockIdx.y * blockDim.y + threadIdx.y;  
    int pixel_pos = idy * w + idx;
    if (idx < w && idy < h) {  
        return;
    }  
}  


unsigned short * depth_pixels_cuda;

void allocate_fix_depth_memory(int w, int h){
    // cudaMalloc((void**)&depth_pixels_cuda, w * h * sizeof(unsigned short));  
}


// color, albedo, normal, fmv, id
void fix_depth(int w, int h, unsigned short * depth_pixels){
    // cudaMemcpy(depth_pixels_cuda, depth_pixels, sizeof(float) * w * h * 4, cudaMemcpyHostToDevice);
    // cudaMemcpy(h_cuda_pictures_4->albedo, host_pictures[1], sizeof(float) * w * h * 4, cudaMemcpyHostToDevice);
    // cudaMemcpy(h_cuda_pictures_4->normal, host_pictures[2], sizeof(float) * w * h * 4, cudaMemcpyHostToDevice);
    // cudaMemcpy(h_cuda_pictures_4->fmv, host_pictures[3], sizeof(float) * w * h * 4, cudaMemcpyHostToDevice);
    // cudaMemcpy(h_cuda_pictures_4->id, host_pictures[4], sizeof(float) * w * h * 4, cudaMemcpyHostToDevice);
    // cudaMemcpy(h_mask->angular_gaze, gaze_data, sizeof(float) * h_mask->m_gaze_length * 2, cudaMemcpyHostToDevice);

    dim3 block(16, 16);  
    dim3 grid((w + block.x - 1) / block.x,   
              (h + block.y - 1) / block.y);  
    
    // convert_4_to_channels<<<grid, block>>>(w, h);  
    // cudaDeviceSynchronize();  
}  