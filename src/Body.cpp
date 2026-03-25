#include "Body.hpp"

Body::Body(int id, double mass, glm::dvec3 pos, glm::dvec3 vel, double radius, Color color, BodyType type)
    : id(id), mass(mass), position(pos), velocity(vel), acceleration(0, 0, 0),
      radius(radius), color(color), type(type) {
}

void Body::updateHistory() {
    if (history.size() >= maxHistory) {
        history.pop_front();
    }
    history.push_back(position);
}
