#include "Integrator.hpp"
#include <cmath>
#include <glm/glm.hpp>
#include <glm/geometric.hpp>

Integrator::Integrator(double G, double softening) : G(G), softening(softening) {}

void Integrator::computeForces(std::vector<Body>& bodies) {
    // Reset accelerations
    for (auto& b : bodies) {
        b.acceleration = glm::dvec3(0, 0, 0);
    }

    double softSq = softening * softening;
    size_t n = bodies.size();

    // O(N^2) / 2 naive force computation with memory optimization
    for (size_t i = 0; i < n; ++i) {
        glm::dvec3 pos_i = bodies[i].position;
        double mass_i = bodies[i].mass;
        glm::dvec3 acc_i(0.0);

        for (size_t j = i + 1; j < n; ++j) {
            glm::dvec3 diff = bodies[j].position - pos_i;
            double distSq = glm::dot(diff, diff);
            // Add softening
            double r2 = distSq + softSq;
            double dist = std::sqrt(r2);
            
            // Force magnitude computation
            double forceMag = G / (r2 * dist);

            glm::dvec3 accel_i = diff * (forceMag * bodies[j].mass);
            glm::dvec3 accel_j = diff * (-forceMag * mass_i);

            acc_i += accel_i;
            bodies[j].acceleration += accel_j;
        }
        bodies[i].acceleration += acc_i;
    }
}

void Integrator::integrate(std::vector<Body>& bodies, double dt) {
    // Semi-implicit Euler
    for (auto& b : bodies) {
        b.velocity += b.acceleration * dt;
        b.position += b.velocity * dt;
    }
}
