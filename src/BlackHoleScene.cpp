#include "BlackHoleScene.hpp"
#include <random>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/geometric.hpp>

namespace {

double computeIscoRadiusFromSchwarzschildRadius(double schwarzschildRadius, double spin) {
    // Use G=c=1 units: r_s = 2M.
    double rs = std::max(schwarzschildRadius, 1e-6);
    double M = 0.5 * rs;

    // Clamp away from |a|=1 singular limit.
    double a = std::clamp(spin, -0.999, 0.999);
    double a2 = a * a;

    double z1 = 1.0 + std::cbrt(1.0 - a2) * (std::cbrt(1.0 + a) + std::cbrt(1.0 - a));
    double z2 = std::sqrt(3.0 * a2 + z1 * z1);
    double signA = (a >= 0.0) ? 1.0 : -1.0;
    double riscoOverM = 3.0 + z2 - signA * std::sqrt((3.0 - z1) * (3.0 + z1 + 2.0 * z2));

    return riscoOverM * M;
}

Color diskParticleColor(double r, double inner, double outer, std::mt19937& gen) {
    double clampedOuter = std::max(outer, inner + 1e-6);
    double t = std::clamp((r - inner) / (clampedOuter - inner), 0.0, 1.0);
    double temp = 1.0 - t;

    glm::dvec3 hot(1.0, 1.0, 0.95);
    glm::dvec3 warm(1.0, 0.65, 0.15);
    glm::dvec3 cool(0.5, 0.12, 0.02);

    glm::dvec3 c;
    if (temp > 0.5) {
        c = glm::mix(warm, hot, (temp - 0.5) * 2.0);
    } else {
        c = glm::mix(cool, warm, temp * 2.0);
    }

    std::uniform_real_distribution<> jitter(0.9, 1.1);
    c *= jitter(gen);
    c = glm::clamp(c, 0.0, 1.0);

    return Color(
        static_cast<float>(c.x),
        static_cast<float>(c.y),
        static_cast<float>(c.z),
        0.9f
    );
}

} // namespace

BlackHoleScene::BlackHoleScene(int numBodies, bool preferCudaBackend)
    : numBodies(numBodies), preferCudaBackend(preferCudaBackend)
#if defined(SIM_ENABLE_CUDA)
    , cudaIntegrator(1.0, 0.5)
#endif
{
    integrator.G = 1.0;
    integrator.softening = 0.5; // smaller softening since extreme forces are intended nearby

#if defined(SIM_ENABLE_CUDA)
    cudaBackendActive = preferCudaBackend && isCudaBackendAvailable();
    if (cudaBackendActive) {
        std::cout << "BlackHoleScene: CUDA backend enabled.\n";
    } else if (preferCudaBackend) {
        std::cout << "BlackHoleScene: CUDA unavailable, falling back to CPU integrator.\n";
    }
#else
    cudaBackendActive = false;
    if (preferCudaBackend) {
        std::cout << "BlackHoleScene: built without CUDA support, using CPU integrator.\n";
    }
#endif
}

void BlackHoleScene::init() {
    bodies.clear();
    double diskInner = getEffectiveDiskInnerRadius();
    double diskOuter = std::max(diskOuterRadius, diskInner + 0.01);

    // Create the black hole
    bodies.emplace_back(
        0,
        blackHoleMass,
        glm::dvec3(0, 0, 0),
        glm::dvec3(0, 0, 0),
        eventHorizon,
        Color(0.1f, 0.1f, 0.1f, 1.0f),
        BodyType::BlackHole
    );

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> disRadius(diskInner, diskOuter);
    std::uniform_real_distribution<> disAngle(0.0, 2.0 * 3.1415926535);
    std::uniform_real_distribution<> disY(-diskThickness, diskThickness); // Thick disk around y=0
    std::uniform_real_distribution<> disMass(0.1, 3.0);

    // Create orbiting bodies
    for (int i = 1; i <= numBodies; ++i) {
        double r = disRadius(gen);
        double theta = disAngle(gen);
        double x = r * std::cos(theta);
        double z = r * std::sin(theta);
        double y = disY(gen);

        // Orbital velocity for circular orbit in XZ plane: v = sqrt(G*M/r)
        double v = std::sqrt(integrator.G * blackHoleMass / r);
        double vx = -v * std::sin(theta);
        double vy = 0.0;
        double vz = v * std::cos(theta);

        double m = disMass(gen);
        double radius = std::pow(m, 0.33) * 0.08;
        Color particleColor = diskParticleColor(r, diskInner, diskOuter, gen);

        bodies.emplace_back(
            i,
            m,
            glm::dvec3(x, y, z),
            glm::dvec3(vx, vy, vz),
            radius,
            particleColor,
            BodyType::Particle
        );
    }

#if defined(SIM_ENABLE_CUDA)
    if (cudaBackendActive) {
        cudaIntegrator.markDeviceStateDirty();
    }
#endif
}

void BlackHoleScene::reset() {
    init();
}

void BlackHoleScene::update(double dt) {
    // Keep black hole locked at origin (index 0)
    if (!bodies.empty() && bodies[0].type == BodyType::BlackHole) {
        bodies[0].position = glm::dvec3(0, 0, 0);
        bodies[0].velocity = glm::dvec3(0, 0, 0);
    }

#if defined(SIM_ENABLE_CUDA)
    if (cudaBackendActive) {
        cudaIntegrator.computeForcesAndIntegrate(bodies, dt);
    } else
#endif
    {
        integrator.computeForces(bodies);
        integrator.integrate(bodies, dt);
    }

    // Enforce lock after integration so the black hole cannot drift.
    if (!bodies.empty() && bodies[0].type == BodyType::BlackHole) {
        bodies[0].position = glm::dvec3(0, 0, 0);
        bodies[0].velocity = glm::dvec3(0, 0, 0);
        bodies[0].acceleration = glm::dvec3(0, 0, 0);
    }

    // Absorption logic and history updating
    auto it = bodies.begin();
    bool removedBody = false;
    if (it != bodies.end() && it->type == BodyType::BlackHole) {
        Body& bh = *it;
        ++it;
        while (it != bodies.end()) {
            glm::dvec3 diff = it->position - bh.position;
            double distSq = glm::dot(diff, diff);
            if (distSq < eventHorizon * eventHorizon) {
                // Absorb mass (optional feature, maybe increase BH mass)
                bh.mass += it->mass;
                it = bodies.erase(it);
                removedBody = true;
            } else {
                it->updateHistory();
                ++it;
            }
        }
        bh.updateHistory();
    } else {
        // Just standard update if no black hole
        for (auto& b : bodies) {
            b.updateHistory();
        }
    }

#if defined(SIM_ENABLE_CUDA)
    if (cudaBackendActive && removedBody) {
        cudaIntegrator.markDeviceStateDirty();
    }
#endif
}

bool BlackHoleScene::getBlackHoleRenderParams(BlackHoleRenderParams& outParams) const {
    if (bodies.empty() || bodies[0].type != BodyType::BlackHole) {
        return false;
    }

    double diskInner = getEffectiveDiskInnerRadius();
    double diskOuter = std::max(diskOuterRadius, diskInner + 0.01);

    outParams.position = glm::vec3(bodies[0].position);
    outParams.schwarzschildRadius = static_cast<float>(eventHorizon);
    outParams.diskInnerRadius = static_cast<float>(diskInner);
    outParams.diskOuterRadius = static_cast<float>(diskOuter);
    outParams.diskThickness = static_cast<float>(diskThickness);
    return true;
}

double BlackHoleScene::getEffectiveDiskInnerRadius() const {
    double inner = diskInnerRadius;
    if (autoComputeDiskInnerRadius) {
        inner = computeIscoRadiusFromSchwarzschildRadius(eventHorizon, blackHoleSpin);
    }

    double minInner = std::max(eventHorizon * 1.02, 0.001);
    double maxInner = std::max(minInner + 0.01, diskOuterRadius - 0.01);
    return std::clamp(inner, minInner, maxInner);
}
