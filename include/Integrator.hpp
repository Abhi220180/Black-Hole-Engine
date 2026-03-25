#pragma once
#include <vector>
#include "Body.hpp"

class Integrator {
public:
    double G = 1.0;          // Gravitational constant
    double softening = 1.0;  // Softening parameter to prevent singularity

    Integrator(double G = 1.0, double softening = 1.0);

    void computeForces(std::vector<Body>& bodies);
    void integrate(std::vector<Body>& bodies, double dt);
};
