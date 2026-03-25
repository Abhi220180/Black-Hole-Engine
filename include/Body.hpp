#pragma once
#include <glm/glm.hpp>
#include <deque>

enum class BodyType {
    Particle,
    Planet,
    Star,
    BlackHole
};

struct Color {
    float r, g, b, a;
    Color(float r=1, float g=1, float b=1, float a=1) : r(r), g(g), b(b), a(a) {}
};

class Body {
public:
    int id;
    double mass;
    glm::dvec3 position;
    glm::dvec3 velocity;
    glm::dvec3 acceleration;
    double radius;
    Color color;
    BodyType type;

    // Trail history
    std::deque<glm::dvec3> history;
    size_t maxHistory = 50;

    Body(int id, double mass, glm::dvec3 pos, glm::dvec3 vel, double radius, Color color, BodyType type = BodyType::Particle);

    void updateHistory();
};
