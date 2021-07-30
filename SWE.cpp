#include <iostream>

#include "types.h"
#include <stdlib.h>
#include <stdio.h>

#define BLOCKSIZE_X 16
#define BLOCKSIZE_Y 16

#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif

#define grid2Dwrite(array, x, y, pitch, value) array[(y)*pitch+x] = value
#define grid2Dread(array, x, y, pitch) array[(y)*pitch+x]

typedef uchar4 rgb;
typedef float4 vertex; // x, h, z
typedef float4 gridpoint; // h, uh, vh, b
typedef int reflection;

const int UNINTIALISED = 0;
const int INITIALISED = 1;

int state = UNINTIALISED;

int stepsperframe = 50;

const float GRAVITY = 9.83219f * 0.5f; //dejar grav cmo variable

const float NN = 5.0f;

const float timestep = 0.01f;

texture<gridpoint, 2, cudaReadModeElementType> texture_grid;
texture<vertex, 2, cudaReadModeElementType> texture_landscape;

int grid_pitch_elements;
cudaChannelFormatDesc grid_channeldesc;

gridpoint* device_grid;
gridpoint* device_grid_next;

vertex* device_heightmap;
vertex* device_watersurfacevertices;

float* device_waves;
rgb* device_watersurfacecolors;

#define EPSILON 0.0001f

__host__ __device__ gridpoint HorizontalPotential(gridpoint gp)
{
    float h = max(gp.x, 0.0f);
    float uh = gp.y;
    float vh = gp.z;

    float h4 = h * h * h * h;
    float u = sqrtf(2.0f) * h * uh / (sqrtf(h4 + max(h4, EPSILON)));

    gridpoint F;
    F.x = u * h;
    F.y = uh * u + GRAVITY * h * h;
    F.z = vh * u;
    F.w = 0.0f;
    return F;
}

__host__ __device__ gridpoint VerticalPotential(gridpoint gp)
{
    float h = max(gp.x, 0.0f);
    float uh = gp.y;
    float vh = gp.z;

    float h4 = h * h * h * h;
    float v = sqrtf(2.0f) * h * vh / (sqrtf(h4 + max(h4, EPSILON)));

    gridpoint G;
    G.x = v * h;
    G.y = uh * v;
    G.z = vh * v + GRAVITY * h * h;
    G.w = 0.0f;
    return G;
}

__host__ __device__ gridpoint SlopeForce(gridpoint c, gridpoint n, gridpoint e, gridpoint s, gridpoint w)
{
    float h = max(c.x, 0.0f);

    gridpoint H;
    H.x = 0.0f;
    H.y = -GRAVITY * h * (e.w - w.w);
    H.z = -GRAVITY * h * (s.w - n.w);
    H.w = 0.0f;
    return H;
}

__host__ __device__ gridpoint operator +(const gridpoint& x, const gridpoint& y)
{
    gridpoint z;
    z.x = x.x + y.x;
    z.y = x.y + y.y;
    z.z = x.z + y.z;
    z.w = x.w + y.w;
    return z;
}
__host__ __device__ gridpoint operator -(const gridpoint& x, const gridpoint& y)
{
    gridpoint z;
    z.x = x.x - y.x;
    z.y = x.y - y.y;
    z.z = x.z - y.z;
    z.w = x.w - y.w;
    return z;
}
__host__ __device__ gridpoint operator *(const gridpoint& x, const gridpoint& y)
{
    gridpoint z;
    z.x = y.x * x.x;
    z.y = y.y * x.y;
    z.z = y.z * x.z;
    z.w = y.w * x.w;
    return z;
}
__host__ __device__ gridpoint operator *(const gridpoint& x, const float& c)
{
    gridpoint z;
    z.x = c * x.x;
    z.y = c * x.y;
    z.z = c * x.z;
    z.w = c * x.w;
    return z;
}
__host__ __device__ gridpoint operator *(const float& c, const gridpoint& x)
{
    return x * c;
}

__host__ __device__ void fixShore(gridpoint& l, gridpoint& c, gridpoint& r)
{

    if(r.x < 0.0f || l.x < 0.0f || c.x < 0.0f)
    {
        c.x = c.x + l.x + r.x;
        c.x = max(0.0f, c.x);
        l.x = 0.0f;
        r.x = 0.0f;
    }
    float h = c.x;
    float h4 = h * h * h * h;
    float v = sqrtf(2.0f) * h * c.y / (sqrtf(h4 + max(h4, EPSILON)));
    float u = sqrtf(2.0f) * h * c.z / (sqrtf(h4 + max(h4, EPSILON)));

    c.y = u * h;
    c.z = v * h;
}

__global__ void simulateWaveStep(gridpoint* grid_next, int width, int height, float timestep, int pitch)
{
    int x = threadIdx.x + blockIdx.x * blockDim.x;
    int y = threadIdx.y + blockIdx.y * blockDim.y;

    if(x < width && y < height)
    {
        int gridx = x + 1;
        int gridy = y + 1;

        gridpoint center = tex2D(texture_grid, gridx, gridy);

        gridpoint north = tex2D(texture_grid, gridx, gridy - 1);

        gridpoint west = tex2D(texture_grid, gridx - 1, gridy);

        gridpoint south = tex2D(texture_grid, gridx, gridy + 1);

        gridpoint east = tex2D(texture_grid, gridx + 1, gridy);

        fixShore(west, center, east);
        fixShore(north, center, south);

        gridpoint u_south = 0.5f * ( south + center ) - timestep * ( VerticalPotential(south) - VerticalPotential(center) );
        gridpoint u_north = 0.5f * ( north + center ) - timestep * ( VerticalPotential(center) - VerticalPotential(north) );
        gridpoint u_west = 0.5f * ( west + center ) - timestep * ( HorizontalPotential(center) - HorizontalPotential(west) );
        gridpoint u_east = 0.5f * ( east + center ) - timestep * ( HorizontalPotential(east) - HorizontalPotential(center) );

        gridpoint u_center = center + timestep * SlopeForce(center, north, east, south, west) - timestep * ( HorizontalPotential(u_east) - HorizontalPotential(u_west) ) - timestep * ( VerticalPotential(u_south) - VerticalPotential(u_north) );
        u_center.x = max(0.0f, u_center.x);
        grid2Dwrite(grid_next, gridx, gridy, pitch, u_center);
    }
}

__global__ void initGrid(gridpoint *grid, int gridwidth, int gridheight, int pitch)
{
    int x = threadIdx.x + blockIdx.x * blockDim.x;
    int y = threadIdx.y + blockIdx.y * blockDim.y;
    if(x < gridwidth && y < gridheight)
    {
        float a = tex2D(texture_landscape, x - 1, y - 1).y;

        gridpoint gp;
        gp.x = max(NN - a, 0.0f);
        gp.y = 0.0f;
        gp.z = 0.0f;
        gp.w = a;
        grid2Dwrite(grid, x, y, pitch, gp);
    }
}

__host__ __device__ vertex gridpointToVertex(gridpoint gp, float x, float y)
{
    float h = gp.x;
    vertex v;
    v.x = x * 20.0f - 10.0f;
    v.z = y * 20.0f - 10.0f;
    v.y = h + gp.w;
    return v;
}

__host__ __device__ rgb gridpointToColor(gridpoint gp)
{
    rgb c;
    c.x = min(20 + (gp.x + gp.w - NN) * 500.0f, 255);
    c.y = min(40 + (gp.x + gp.w - NN) * 500.0f, 255);
    c.z = min(100 + (gp.x + gp.w - NN) * 500.0f, 255);
    c.w = 255 - max(-50 * gp.x + 50, 0);
    return c;
}

__global__ void visualise(vertex* watersurfacevertices,
                          rgb* watersurfacecolors, int width, int height)
                          {
    int x = threadIdx.x + blockIdx.x * blockDim.x;
    int y = threadIdx.y + blockIdx.y * blockDim.y;

    if(x < width && y < height)
    {
        int gridx = x + 1;
        int gridy = y + 1;

        gridpoint gp = tex2D(texture_grid, gridx, gridy);

        watersurfacevertices[y * width + x] = gridpointToVertex(gp, x / float(width - 1), y / float(height - 1));
        watersurfacecolors[y * width + x] = gridpointToColor(gp);
    }
                          }


                          __global__ void addWave(gridpoint* grid, float* wave, float norm, int width, int height, int pitch)
                          {
    int x = threadIdx.x + blockIdx.x * blockDim.x;
    int y = threadIdx.y + blockIdx.y * blockDim.y;

    if(x < width && y < height)
    {
        int gridx = x + 1;
        int gridy = y + 1;

        float waveheight = grid2Dread(grid, gridx, gridy, pitch).x;

        float toadd = max(grid2Dread(wave, x, y, width) - 0.2f, 0.0f) * norm;
        waveheight += toadd;
        grid[ gridx + gridy * pitch ].x = waveheight;

    }
                          }

                          void addWave(float* wave, float norm, int width, int height, int pitch_elements)
                          {
    cudaError_t error;
    size_t sizeInBytes = width * height * sizeof(float);

    error = cudaMemcpy(device_waves, wave, sizeInBytes, cudaMemcpyHostToDevice);
    CHECK_EQ(cudaSuccess, error) << "Error: " << cudaGetErrorString(error);

    int x = (width + BLOCKSIZE_X - 1) / BLOCKSIZE_X;
    int y = (height + BLOCKSIZE_Y - 1) / BLOCKSIZE_Y;
    dim3 threadsPerBlock(BLOCKSIZE_X, BLOCKSIZE_Y);
    dim3 blocksPerGrid(x, y);

    addWave <<< blocksPerGrid, threadsPerBlock>>>(device_grid_next, device_waves, norm, width, height, pitch_elements);

    error = cudaGetLastError();
    CHECK_EQ(cudaSuccess, error) << "Error: " << cudaGetErrorString(error);
    error = cudaThreadSynchronize();
    CHECK_EQ(cudaSuccess, error) << "Error: " << cudaGetErrorString(error);

    gridpoint *grid_helper = device_grid;
    device_grid = device_grid_next;
    device_grid_next = grid_helper;

    error = cudaBindTexture2D(0, &texture_grid, device_grid, &grid_channeldesc, width + 2, height + 2, grid_pitch_elements * sizeof(gridpoint));
    CHECK_EQ(cudaSuccess, error) << "Error at line " << __LINE__ << ": " << cudaGetErrorString(error);
                          }

                          void initWaterSurface(int width, int height, vertex *heightmapvertices, float *wave)
                          {

    if(state != UNINTIALISED)
    {
        return;
    }
    int gridwidth = width + 2;
    int gridheight = height + 2;

    size_t sizeInBytes;
    size_t grid_pitch;
    cudaError_t error;

    grid_channeldesc = cudaCreateChannelDesc<float4>();
    cudaChannelFormatDesc treshholds_channeldesc = cudaCreateChannelDesc<float>();
    cudaChannelFormatDesc reflections_channeldesc = cudaCreateChannelDesc<int>();

    //alloc pitched memory for device_grid
    error = cudaMallocPitch(&device_grid, &grid_pitch, gridwidth * sizeof(gridpoint), gridheight);
    CHECK_EQ(cudaSuccess, error) << "Error: " << cudaGetErrorString(error);
    CHECK_NOTNULL(device_grid);

    size_t oldpitch = grid_pitch;

    //alloc pitched memoty for device_grid_next
    error = cudaMallocPitch(&device_grid_next, &grid_pitch, gridwidth * sizeof(gridpoint), gridheight);
    CHECK_EQ(cudaSuccess, error) << "Error: " << cudaGetErrorString(error);
    CHECK_NOTNULL(device_grid_next);

    CHECK_EQ(oldpitch, grid_pitch);

    grid_pitch_elements = grid_pitch / sizeof(gridpoint);

    //alloc pitched memory for landscape data on device
    size_t heightmap_pitch;
    error = cudaMallocPitch(&device_heightmap, &heightmap_pitch, width * sizeof(vertex), height);
    CHECK_EQ(cudaSuccess, error) << "Error: " << cudaGetErrorString(error);
    CHECK_NOTNULL(device_heightmap);

    // copy landscape data to device
    error = cudaMemcpy2D(device_heightmap, heightmap_pitch, heightmapvertices, width * sizeof(vertex), width * sizeof(vertex), height, cudaMemcpyHostToDevice);
    CHECK_EQ(cudaSuccess, error) << "Error: " << cudaGetErrorString(error);

    // bind heightmap to texture_landscape
    cudaChannelFormatDesc heightmap_channeldesc = cudaCreateChannelDesc<float4>();
    error = cudaBindTexture2D(0, &texture_landscape, device_heightmap, &heightmap_channeldesc, width, height, heightmap_pitch);
    CHECK_EQ(cudaSuccess, error) << "Error at line " << __LINE__ << ": " << cudaGetErrorString(error);

    // malloc memory for watersurface vertices
    sizeInBytes = width * height * sizeof(vertex);
    error = cudaMalloc(&device_watersurfacevertices, sizeInBytes);
    CHECK_EQ(cudaSuccess, error) << "Error: " << cudaGetErrorString(error);

    // malloc memory for watersurface colors
    sizeInBytes = height * width * sizeof(rgb);
    error = cudaMalloc(&device_watersurfacecolors, sizeInBytes);
    CHECK_EQ(cudaSuccess, error) << "Error: " << cudaGetErrorString(error);

    // malloc memory for waves
    sizeInBytes = height * width * sizeof(float);
    error = cudaMalloc(&device_waves, sizeInBytes);
    CHECK_EQ(cudaSuccess, error) << "Error: " << cudaGetErrorString(error);

    // make dimension
    int x = (gridwidth + BLOCKSIZE_X - 1) / BLOCKSIZE_X;
    int y = (gridheight + BLOCKSIZE_Y - 1) / BLOCKSIZE_Y;
    dim3 threadsPerBlock(BLOCKSIZE_X, BLOCKSIZE_Y);
    dim3 blocksPerGrid(x, y);

    int x1 = (gridwidth + BLOCKSIZE_X - 1) / BLOCKSIZE_X;
    dim3 threadsPerBlock1(BLOCKSIZE_X, 1);
    dim3 blocksPerGrid1(x1, 1);

    int y1 = (gridheight + BLOCKSIZE_Y -  1) / BLOCKSIZE_Y;
    dim3 threadsPerBlock2(1, BLOCKSIZE_Y);
    dim3 blocksPerGrid2(1, y1);

    //init grid with initial values
    initGrid <<< blocksPerGrid, threadsPerBlock>>>(device_grid, gridwidth, gridheight, grid_pitch_elements);

    //init grid_next with initial values
    initGrid <<< blocksPerGrid, threadsPerBlock>>>(device_grid_next, gridwidth, gridheight, grid_pitch_elements);

    error = cudaThreadSynchronize();
    CHECK_EQ(cudaSuccess, error) << "Error: " << cudaGetErrorString(error);


    //bind the grid to texture_grid
    error = cudaBindTexture2D(0, &texture_grid, device_grid, &grid_channeldesc, gridwidth, gridheight, grid_pitch);
    CHECK_EQ(cudaSuccess, error) << "Error at line " << __LINE__ << ": " << cudaGetErrorString(error);

    //add the initial wave to the grid
    addWave(wave, 5.0f, width, height, grid_pitch_elements);

    state = INITIALISED;
                          }

                          void computeNext(int width, int height, vertex* watersurfacevertices, rgb* watersurfacecolors, int steps)
                          {
    if(state != INITIALISED)
    {
        return;
    }

    int gridwidth = width + 2;
    int gridheight = height + 2;

    cudaError_t error;
    // make dimension
    int x = (width + BLOCKSIZE_X - 1) / BLOCKSIZE_X;
    int y = (height + BLOCKSIZE_Y - 1) / BLOCKSIZE_Y;
    dim3 threadsPerBlock(BLOCKSIZE_X, BLOCKSIZE_Y);
    dim3 blocksPerGrid(x, y);

    //gitter "stepsperframe" zeitschritt
    for(int x = 0; x < steps; x++)
    {
        simulateWaveStep <<< blocksPerGrid, threadsPerBlock>>>(device_grid_next, width, height, timestep, grid_pitch_elements);

        error = cudaThreadSynchronize();
        CHECK_EQ(cudaSuccess, error) << "Error: " << cudaGetErrorString(error);

        gridpoint *grid_helper = device_grid;
        device_grid = device_grid_next;
        device_grid_next = grid_helper;

        error = cudaBindTexture2D(0, &texture_grid, device_grid, &grid_channeldesc, gridwidth, gridheight, grid_pitch_elements * sizeof(gridpoint));
        CHECK_EQ(cudaSuccess, error) << "Error at line " << __LINE__ << ": " << cudaGetErrorString(error);
    }
    visualise <<< blocksPerGrid, threadsPerBlock >>>(device_watersurfacevertices, device_watersurfacecolors, width, height);

    error = cudaGetLastError();
    CHECK_EQ(cudaSuccess, error) << "Error: " << cudaGetErrorString(error);
    error = cudaThreadSynchronize();
    CHECK_EQ(cudaSuccess, error) << "Error: " << cudaGetErrorString(error);

    // copy back data
    error = cudaMemcpy(watersurfacevertices, device_watersurfacevertices, width * height * sizeof(vertex), cudaMemcpyDeviceToHost);
    CHECK_EQ(cudaSuccess, error) << "Error: " << cudaGetErrorString(error);
    error = cudaMemcpy(watersurfacecolors, device_watersurfacecolors, width * height * sizeof(rgb), cudaMemcpyDeviceToHost);
    CHECK_EQ(cudaSuccess, error) << "Error: " << cudaGetErrorString(error);
                          }

                          void destroyWaterSurface()
                          {
    if(state != INITIALISED)
    {
        return;
    }
    cudaUnbindTexture(texture_grid);
    cudaUnbindTexture(texture_landscape);
    cudaFree(device_grid);
    cudaFree(device_grid_next);

    cudaFree(device_heightmap);
    cudaFree(device_watersurfacevertices);

    cudaFree(device_waves);
    cudaFree(device_watersurfacecolors);
    state = UNINTIALISED;
                          }