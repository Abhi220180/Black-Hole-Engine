#pragma once
#include <string>
#include "Scene.hpp"
#if defined(SIM_ENABLE_CUDA)
#include "CudaIntegrator.cuh"
#endif

class BlackHoleScene : public Scene {
public:
    int numBodies = 200;
    double blackHoleMass = 1000.0;
    double eventHorizon = 2.0;
    double blackHoleSpin = 0.0; // Kerr spin parameter in [-1, 1]

    bool autoComputeDiskInnerRadius = true;
    double diskInnerRadius = 5.0;
    double diskOuterRadius = 30.0;
    double diskThickness = 1.0;

    explicit BlackHoleScene(int numBodies = 200, bool preferCudaBackend = true);

    void init() override;
    void update(double dt) override;
    void reset() override;

    SceneRenderMode getRenderMode() const override { return SceneRenderMode::BlackHoleLensing; }
    bool getBlackHoleRenderParams(BlackHoleRenderParams& outParams) const override;

    bool isUsingCudaBackend() const { return cudaBackendActive; }
    std::string getName() const override { return "Black Hole Simulation"; }

private:
    bool preferCudaBackend = true;
    bool cudaBackendActive = false;

    double getEffectiveDiskInnerRadius() const;

#if defined(SIM_ENABLE_CUDA)
    CudaIntegrator cudaIntegrator;
#endif
};
