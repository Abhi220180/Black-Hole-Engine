#pragma once
#include "Body.hpp"
#include "Integrator.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <string>

enum class SceneRenderMode {
    Standard,
    BlackHoleLensing
};

struct BlackHoleRenderParams {
    glm::vec3 position = glm::vec3(0.0f);
    float schwarzschildRadius = 2.0f;
    float diskInnerRadius = 5.0f;
    float diskOuterRadius = 30.0f;
    float diskThickness = 1.0f;
};

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

    virtual SceneRenderMode getRenderMode() const {
        return SceneRenderMode::Standard;
    }

    virtual bool getBlackHoleRenderParams(BlackHoleRenderParams& outParams) const {
        (void)outParams;
        return false;
    }

    virtual std::string getName() const = 0;
};
