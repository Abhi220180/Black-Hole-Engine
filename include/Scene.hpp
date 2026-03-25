#pragma once
#include "Body.hpp"
#include "Integrator.hpp"
#include <vector>
#include <string>

class Scene {
protected:
    std::vector<Body> bodies;
    Integrator integrator;
public:
    virtual ~Scene() = default;

    virtual void init() = 0;
    virtual void update(double dt) = 0;
    virtual void reset() = 0;

    const std::vector<Body>& getBodies() const {
        return bodies;
    }

    virtual std::string getName() const = 0;
};
