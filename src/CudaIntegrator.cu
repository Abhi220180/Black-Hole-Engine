#include <cuda_runtime.h>
#include <cstdio>
#include <cstdlib>

namespace {

static constexpr int BLOCK_SIZE = 128;

static double* d_posX = nullptr;
static double* d_posY = nullptr;
static double* d_posZ = nullptr;
static double* d_velX = nullptr;
static double* d_velY = nullptr;
static double* d_velZ = nullptr;
static double* d_accX = nullptr;
static double* d_accY = nullptr;
static double* d_accZ = nullptr;
static double* d_mass = nullptr;
static int allocatedN = 0;
static int cudaAvailability = -1;

bool cudaCheck(cudaError_t err, const char* file, int line) {
    if (err == cudaSuccess) {
        return true;
    }

    std::fprintf(stderr, "CUDA error at %s:%d - %s\n", file, line, cudaGetErrorString(err));
    return false;
}

#define CUDA_CHECK_BOOL(call) do { if (!cudaCheck((call), __FILE__, __LINE__)) return false; } while (0)

void freeDeviceMemoryInternal() {
    if (allocatedN == 0) {
        return;
    }

    cudaFree(d_posX);
    cudaFree(d_posY);
    cudaFree(d_posZ);
    cudaFree(d_velX);
    cudaFree(d_velY);
    cudaFree(d_velZ);
    cudaFree(d_accX);
    cudaFree(d_accY);
    cudaFree(d_accZ);
    cudaFree(d_mass);

    d_posX = d_posY = d_posZ = nullptr;
    d_velX = d_velY = d_velZ = nullptr;
    d_accX = d_accY = d_accZ = nullptr;
    d_mass = nullptr;
    allocatedN = 0;
}

bool ensureDeviceMemoryInternal(int N) {
    if (N <= allocatedN) {
        return true;
    }

    freeDeviceMemoryInternal();

    size_t bytes = static_cast<size_t>(N) * sizeof(double);
    CUDA_CHECK_BOOL(cudaMalloc(&d_posX, bytes));
    CUDA_CHECK_BOOL(cudaMalloc(&d_posY, bytes));
    CUDA_CHECK_BOOL(cudaMalloc(&d_posZ, bytes));
    CUDA_CHECK_BOOL(cudaMalloc(&d_velX, bytes));
    CUDA_CHECK_BOOL(cudaMalloc(&d_velY, bytes));
    CUDA_CHECK_BOOL(cudaMalloc(&d_velZ, bytes));
    CUDA_CHECK_BOOL(cudaMalloc(&d_accX, bytes));
    CUDA_CHECK_BOOL(cudaMalloc(&d_accY, bytes));
    CUDA_CHECK_BOOL(cudaMalloc(&d_accZ, bytes));
    CUDA_CHECK_BOOL(cudaMalloc(&d_mass, bytes));

    allocatedN = N;
    return true;
}

} // namespace

bool isCudaBackendAvailable() {
    if (cudaAvailability >= 0) {
        return cudaAvailability == 1;
    }

    int deviceCount = 0;
    cudaError_t status = cudaGetDeviceCount(&deviceCount);
    if (status == cudaSuccess && deviceCount > 0) {
        cudaAvailability = 1;
        return true;
    }

    cudaAvailability = 0;
    return false;
}

__global__ void computeForcesKernel(
    const double* __restrict__ posX,
    const double* __restrict__ posY,
    const double* __restrict__ posZ,
    const double* __restrict__ mass,
    double* __restrict__ accX,
    double* __restrict__ accY,
    double* __restrict__ accZ,
    int N,
    double G,
    double softeningSq,
    int lockedIndex)
{
    __shared__ double sPosX[BLOCK_SIZE];
    __shared__ double sPosY[BLOCK_SIZE];
    __shared__ double sPosZ[BLOCK_SIZE];
    __shared__ double sMass[BLOCK_SIZE];

    int i = blockIdx.x * blockDim.x + threadIdx.x;
    bool isLocked = (i == lockedIndex);
    bool isActive = (i < N) && !isLocked;

    double myPosX = 0.0;
    double myPosY = 0.0;
    double myPosZ = 0.0;
    double ax = 0.0;
    double ay = 0.0;
    double az = 0.0;

    if (i < N) {
        myPosX = posX[i];
        myPosY = posY[i];
        myPosZ = posZ[i];
    }

    int numTiles = (N + BLOCK_SIZE - 1) / BLOCK_SIZE;
    for (int tile = 0; tile < numTiles; ++tile) {
        int idx = tile * BLOCK_SIZE + threadIdx.x;

        if (idx < N) {
            sPosX[threadIdx.x] = posX[idx];
            sPosY[threadIdx.x] = posY[idx];
            sPosZ[threadIdx.x] = posZ[idx];
            sMass[threadIdx.x] = mass[idx];
        } else {
            sPosX[threadIdx.x] = 0.0;
            sPosY[threadIdx.x] = 0.0;
            sPosZ[threadIdx.x] = 0.0;
            sMass[threadIdx.x] = 0.0;
        }
        __syncthreads();

        if (isActive) {
            int tileEnd = min(BLOCK_SIZE, N - tile * BLOCK_SIZE);
            for (int k = 0; k < tileEnd; ++k) {
                int j = tile * BLOCK_SIZE + k;
                if (j == i) {
                    continue;
                }

                double dx = sPosX[k] - myPosX;
                double dy = sPosY[k] - myPosY;
                double dz = sPosZ[k] - myPosZ;
                double distSq = dx * dx + dy * dy + dz * dz + softeningSq;
                double dist = sqrt(distSq);
                double invDist3 = G * sMass[k] / (distSq * dist);
                ax += dx * invDist3;
                ay += dy * invDist3;
                az += dz * invDist3;
            }
        }
        __syncthreads();
    }

    if (i < N) {
        if (isLocked) {
            accX[i] = 0.0;
            accY[i] = 0.0;
            accZ[i] = 0.0;
        } else {
            accX[i] = ax;
            accY[i] = ay;
            accZ[i] = az;
        }
    }
}

__global__ void integrateKernel(
    double* __restrict__ posX,
    double* __restrict__ posY,
    double* __restrict__ posZ,
    double* __restrict__ velX,
    double* __restrict__ velY,
    double* __restrict__ velZ,
    const double* __restrict__ accX,
    const double* __restrict__ accY,
    const double* __restrict__ accZ,
    int N,
    double dt,
    int lockedIndex)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= N) {
        return;
    }

    if (i == lockedIndex) {
        posX[i] = 0.0;
        posY[i] = 0.0;
        posZ[i] = 0.0;
        velX[i] = 0.0;
        velY[i] = 0.0;
        velZ[i] = 0.0;
        return;
    }

    velX[i] += accX[i] * dt;
    velY[i] += accY[i] * dt;
    velZ[i] += accZ[i] * dt;

    posX[i] += velX[i] * dt;
    posY[i] += velY[i] * dt;
    posZ[i] += velZ[i] * dt;
}

bool cudaUploadPhysicsState(
    int N,
    const double* hPosX,
    const double* hPosY,
    const double* hPosZ,
    const double* hVelX,
    const double* hVelY,
    const double* hVelZ,
    const double* hMass)
{
    if (N == 0) {
        return true;
    }
    if (!isCudaBackendAvailable()) {
        return false;
    }
    if (!ensureDeviceMemoryInternal(N)) {
        return false;
    }

    size_t bytes = static_cast<size_t>(N) * sizeof(double);
    CUDA_CHECK_BOOL(cudaMemcpy(d_posX, hPosX, bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK_BOOL(cudaMemcpy(d_posY, hPosY, bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK_BOOL(cudaMemcpy(d_posZ, hPosZ, bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK_BOOL(cudaMemcpy(d_velX, hVelX, bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK_BOOL(cudaMemcpy(d_velY, hVelY, bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK_BOOL(cudaMemcpy(d_velZ, hVelZ, bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK_BOOL(cudaMemcpy(d_mass, hMass, bytes, cudaMemcpyHostToDevice));

    return true;
}

bool cudaIntegratePhysicsState(int N, double G, double softening, double dt, int lockedIndex) {
    if (N == 0) {
        return true;
    }
    if (!isCudaBackendAvailable()) {
        return false;
    }
    if (!ensureDeviceMemoryInternal(N)) {
        return false;
    }

    int blocks = (N + BLOCK_SIZE - 1) / BLOCK_SIZE;
    double softeningSq = softening * softening;

    computeForcesKernel<<<blocks, BLOCK_SIZE>>>(
        d_posX,
        d_posY,
        d_posZ,
        d_mass,
        d_accX,
        d_accY,
        d_accZ,
        N,
        G,
        softeningSq,
        lockedIndex);

    integrateKernel<<<blocks, BLOCK_SIZE>>>(
        d_posX,
        d_posY,
        d_posZ,
        d_velX,
        d_velY,
        d_velZ,
        d_accX,
        d_accY,
        d_accZ,
        N,
        dt,
        lockedIndex);

    CUDA_CHECK_BOOL(cudaGetLastError());
    CUDA_CHECK_BOOL(cudaDeviceSynchronize());
    return true;
}

bool cudaDownloadPhysicsState(
    int N,
    double* hPosX,
    double* hPosY,
    double* hPosZ,
    double* hVelX,
    double* hVelY,
    double* hVelZ)
{
    if (N == 0) {
        return true;
    }
    if (!isCudaBackendAvailable()) {
        return false;
    }

    size_t bytes = static_cast<size_t>(N) * sizeof(double);
    CUDA_CHECK_BOOL(cudaMemcpy(hPosX, d_posX, bytes, cudaMemcpyDeviceToHost));
    CUDA_CHECK_BOOL(cudaMemcpy(hPosY, d_posY, bytes, cudaMemcpyDeviceToHost));
    CUDA_CHECK_BOOL(cudaMemcpy(hPosZ, d_posZ, bytes, cudaMemcpyDeviceToHost));
    CUDA_CHECK_BOOL(cudaMemcpy(hVelX, d_velX, bytes, cudaMemcpyDeviceToHost));
    CUDA_CHECK_BOOL(cudaMemcpy(hVelY, d_velY, bytes, cudaMemcpyDeviceToHost));
    CUDA_CHECK_BOOL(cudaMemcpy(hVelZ, d_velZ, bytes, cudaMemcpyDeviceToHost));
    return true;
}

void releaseCudaPhysicsBuffers() {
    freeDeviceMemoryInternal();
}