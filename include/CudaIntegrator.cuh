#pragma once

#include <vector>
#include "Body.hpp"

// CUDA bridge functions implemented in src/CudaIntegrator.cu.
bool cudaUploadPhysicsState(
    int N,
    const double* hPosX, const double* hPosY, const double* hPosZ,
    const double* hVelX, const double* hVelY, const double* hVelZ,
    const double* hMass
);
bool cudaIntegratePhysicsState(int N, double G, double softening, double dt, int lockedIndex);
bool cudaDownloadPhysicsState(
    int N,
    double* hPosX, double* hPosY, double* hPosZ,
    double* hVelX, double* hVelY, double* hVelZ
);
void releaseCudaPhysicsBuffers();

bool isCudaBackendAvailable();

class CudaIntegrator {
public:
    double G        = 1.0;
    double softening = 1.0;

    CudaIntegrator(double G = 1.0, double softening = 1.0);
    ~CudaIntegrator();

    void computeForcesAndIntegrate(std::vector<Body>& bodies, double dt);
    void markDeviceStateDirty();
    bool isDeviceStateResident() const { return deviceStateValid; }

private:
    std::vector<double> hPosX;
    std::vector<double> hPosY;
    std::vector<double> hPosZ;
    std::vector<double> hVelX;
    std::vector<double> hVelY;
    std::vector<double> hVelZ;
    std::vector<double> hMass;

    int cachedN = 0;
    bool deviceStateValid = false;

    void ensureHostBufferSize(int N);
    void packBodiesToHost(const std::vector<Body>& bodies);
    void unpackBodiesFromHost(std::vector<Body>& bodies) const;
};
