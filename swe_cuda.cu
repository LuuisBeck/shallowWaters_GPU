//
// Created by luchin on 29-07-21.
//
#include <cassert>
#include <iostream>

static cudaError_t checkCuda(cudaError_t result) {
    if (result != cudaSuccess) {
        fprintf(stderr, "CUDA Runtime Error: %s\n", cudaGetErrorString(result));
        assert(result == cudaSuccess);
    }
    return result;
}

Par_CUDA::Par_CUDA() : AbstractGoL() {
    int devId = 0;
    cudaDeviceProp prop;
    checkCuda(cudaGetDeviceProperties(&prop, devId));
    checkCuda(cudaSetDevice(devId));
    int total_size = sizeof(char) * LARGO * LARGO;
    d_grid = nullptr;
    checkCuda(cudaMalloc(&d_grid, total_size));
}

__global__ void step(char *grid) {
#ifdef CUDA_USE_2D
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
#else
    int tmp = blockIdx.x * blockDim.x + threadIdx.x;
    int x = tmp / LARGO;
    int y = tmp % LARGO;
#endif

    // contamos los vecinos
    //    printf("x is %d and y is %d\n", x, y);
    if (x > LARGO || y > LARGO) return;
    x += LARGO; // nos aseguramos de que x-1 sea positivo
    y += LARGO;
    int x_m = (x - 1) % LARGO;
    int x_p = (x + 1) % LARGO;
    int y_m = (y - 1) % LARGO;
    int y_p = (y + 1) % LARGO;
    x = x % LARGO;
    y = y % LARGO;
    int num_neighbors =
            grid[x_m * LARGO + y_m] + grid[x * LARGO + y_m] + grid[x_p * LARGO + y_m] +
            grid[x_m * LARGO + y] + grid[x_p * LARGO + y] +
            grid[x_m * LARGO + y_p] + grid[x * LARGO + y_p] + grid[x_p * LARGO + y_p];
    char alive = grid[x * LARGO + y];

    __syncthreads();
    // reemplazamos los lugares donde corresponde
    if ((alive && num_neighbors == 2) || num_neighbors == 3) {
        grid[x * LARGO + y] = 1;
    } else {
        grid[x * LARGO + y] = 0;
    }
}


void Par_CUDA::run_game(int num_steps) {
#ifdef CUDA_USE_2D
    dim3 dimGrid((LARGO + 7) / 8, (LARGO + 7) / 88, 1);
    dim3 dimBlock(8, 8, 1);
#else
    dim3 dimGrid((LARGO * LARGO + 7) / 8, 1, 1);
    dim3 dimBlock(8, 1, 1);
#endif
    cudaMemcpy(d_grid, h_grid, sizeof(char) * LARGO * LARGO, cudaMemcpyHostToDevice);
    for (int i = 0; i < num_steps; i++) {
        step<<<dimGrid, dimBlock>>>(d_grid);
    }
    cudaMemcpy(h_grid, d_grid, sizeof(char) * LARGO * LARGO, cudaMemcpyDeviceToHost);
}