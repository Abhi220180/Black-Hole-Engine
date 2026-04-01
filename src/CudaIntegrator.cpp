#include "CudaIntegrator.cuh"
#include <glm/glm.hpp>
#include <vector>

CudaIntegrator::CudaIntegrator(double G, double softening)
    : G(G), softening(softening) {}

CudaIntegrator::~CudaIntegrator() {
    releaseCudaPhysicsBuffers();
}

void CudaIntegrator::markDeviceStateDirty() {
    deviceStateValid = false;
}

void CudaIntegrator::ensureHostBufferSize(int N) {
    if (N == cachedN) {
        return;
    }

    hPosX.resize(N);
    hPosY.resize(N);
    hPosZ.resize(N);
    hVelX.resize(N);
    hVelY.resize(N);
    hVelZ.resize(N);
    hMass.resize(N);
    cachedN = N;
}

void CudaIntegrator::packBodiesToHost(const std::vector<Body>& bodies) {
    const int N = static_cast<int>(bodies.size());
    ensureHostBufferSize(N);

    for (int i = 0; i < N; ++i) {
        hPosX[i] = bodies[i].position.x;
        hPosY[i] = bodies[i].position.y;
        hPosZ[i] = bodies[i].position.z;
        hVelX[i] = bodies[i].velocity.x;
        hVelY[i] = bodies[i].velocity.y;
        hVelZ[i] = bodies[i].velocity.z;
        hMass[i] = bodies[i].mass;
    }
}

void CudaIntegrator::unpackBodiesFromHost(std::vector<Body>& bodies) const {
    const int N = static_cast<int>(bodies.size());
    for (int i = 0; i < N; ++i) {
        bodies[i].position = glm::dvec3(hPosX[i], hPosY[i], hPosZ[i]);
        bodies[i].velocity = glm::dvec3(hVelX[i], hVelY[i], hVelZ[i]);
    }
}

void CudaIntegrator::computeForcesAndIntegrate(std::vector<Body>& bodies, double dt) {
    const int N = static_cast<int>(bodies.size());
    if (N == 0) {
        return;
    }

    const bool topologyChanged = (N != cachedN);
    ensureHostBufferSize(N);
    if (topologyChanged) {
        deviceStateValid = false;
    }

    if (!deviceStateValid) {
        packBodiesToHost(bodies);
        if (!cudaUploadPhysicsState(
                N,
                hPosX.data(), hPosY.data(), hPosZ.data(),
                hVelX.data(), hVelY.data(), hVelZ.data(),
                hMass.data())) {
            return;
        }
        deviceStateValid = true;
    }

    // Body 0 is the scene black hole and is locked at origin by design.
    if (!cudaIntegratePhysicsState(N, G, softening, dt, 0)) {
        deviceStateValid = false;
        return;
    }

    if (!cudaDownloadPhysicsState(
            N,
            hPosX.data(), hPosY.data(), hPosZ.data(),
            hVelX.data(), hVelY.data(), hVelZ.data())) {
        deviceStateValid = false;
        return;
    }

    unpackBodiesFromHost(bodies);
}
