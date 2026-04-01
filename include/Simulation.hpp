#pragma once
#include "Renderer.hpp"
#include "Scene.hpp"
#include "Exporter.hpp"
#include <memory>
#include <glm/glm.hpp>

class Simulation {
public:
    Simulation();
    ~Simulation();

    void run();

private:
    struct OrbitCameraState {
        bool enabled = false;
        float distance = 45.0f;
        glm::vec3 target = glm::vec3(0.0f);
    };

    Renderer renderer;
    std::unique_ptr<Scene> currentScene;
    Exporter exporter;

    bool paused = false;
    double time = 0.0;
    double dt = 0.01; // fixed timestep for integration
    double accumulator = 0.0;
    double maxFrameTime = 0.1;
    int maxSubstepsPerFrame = 8;

    // Camera (3D Fly Camera)
    glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 50.0f);
    float pitch = 0.0f;
    float yaw = -90.0f;
    OrbitCameraState orbitCamera;
    
    // Mouse state
    double lastX = 0.0;
    double lastY = 0.0;
    bool firstMouse = true;
    bool rightMousePressed = false;

    // scene indices
    int sceneIndex = 0;

    glm::vec3 getOrbitTarget() const;
    void syncOrbitDistanceFromCamera();
    void updateOrbitCameraPosition();

    void processInput(double frameDt);
    void switchScene(int index);
};
