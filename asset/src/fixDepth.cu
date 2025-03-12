#include "fixDepth.h"

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

unsigned short * depth_pixels_cuda;

void allocate_fix_depth_memory(int w, int h){
    cudaMalloc((void**)&depth_pixels_cuda, w * h * sizeof(unsigned short));  
}


// color, albedo, normal, fmv, id
void fix_depth(int w, int h, unsigned short * depth_pixels){
    cudaMemcpy(depth_pixels_cuda, depth_pixels, sizeof(unsigned short) * w * h, cudaMemcpyHostToDevice);
    // cudaMemcpy(h_cuda_pictures_4->albedo, host_pictures[1], sizeof(float) * w * h * 4, cudaMemcpyHostToDevice);
    // cudaMemcpy(h_cuda_pictures_4->normal, host_pictures[2], sizeof(float) * w * h * 4, cudaMemcpyHostToDevice);
    // cudaMemcpy(h_cuda_pictures_4->fmv, host_pictures[3], sizeof(float) * w * h * 4, cudaMemcpyHostToDevice);
    // cudaMemcpy(h_cuda_pictures_4->id, host_pictures[4], sizeof(float) * w * h * 4, cudaMemcpyHostToDevice);
    // cudaMemcpy(h_mask->angular_gaze, gaze_data, sizeof(float) * h_mask->m_gaze_length * 2, cudaMemcpyHostToDevice);

    dim3 block(16, 16);  
    dim3 grid((w + block.x - 1) / block.x,   
              (h + block.y - 1) / block.y);  

    for(int iterate_num = 0; iterate_num < 15; iterate_num++){
        convert_4_to_channels<<<grid, block>>>(w, h, depth_pixels_cuda);  
    }
    cudaDeviceSynchronize();  
    cudaMemcpy(depth_pixels, depth_pixels_cuda, sizeof(unsigned short) * w * h, cudaMemcpyDeviceToHost);
}  