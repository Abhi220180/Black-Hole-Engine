#include "BlackHoleScene.hpp"
#include <random>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/geometric.hpp>

BlackHoleScene::BlackHoleScene(int numBodies) : numBodies(numBodies) {
    integrator.G = 1.0;
    integrator.softening = 0.5; // smaller softening since extreme forces are intended nearby
}

void BlackHoleScene::init() {
    bodies.clear();

    // Create the black hole
    bodies.emplace_back(
        0,
        blackHoleMass,
        glm::dvec3(0, 0, 0),
        glm::dvec3(0, 0, 0),
        eventHorizon,
        Color(0.1, 0.1, 0.1, 1.0),
        BodyType::BlackHole
    );

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> disRadius(5.0, 30.0);
    std::uniform_real_distribution<> disAngle(0.0, 2.0 * 3.1415926535);
    std::uniform_real_distribution<> disZ(-1.0, 1.0); // Thick disk
    std::uniform_real_distribution<> disMass(0.1, 3.0);
    std::uniform_real_distribution<> disColor(0.5, 1.0);

    // Create orbiting bodies
    for (int i = 1; i <= numBodies; ++i) {
        double r = disRadius(gen);
        double theta = disAngle(gen);
        double x = r * std::cos(theta);
        double y = r * std::sin(theta);
        double z = disZ(gen);

        // Orbital velocity for circular orbit in XY plane: v = sqrt(G*M/r)
        double v = std::sqrt(integrator.G * blackHoleMass / r);
        double vx = -v * std::sin(theta);
        double vy = v * std::cos(theta);
        double vz = 0.0; // mostly flat disk

        double m = disMass(gen);
        double radius = std::pow(m, 0.33) * 0.2;

        bodies.emplace_back(
            i,
            m,
            glm::dvec3(x, y, z),
            glm::dvec3(vx, vy, vz),
            radius,
            Color(disColor(gen), disColor(gen), disColor(gen), 1.0),
            BodyType::Particle
        );
    }
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

    integrator.computeForces(bodies);
    integrator.integrate(bodies, dt);

    // Absorption logic and history updating
    auto it = bodies.begin();
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
}
