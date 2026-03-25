#include "Simulation.hpp"
#include "BlackHoleScene.hpp"
#include <glm/glm.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

Simulation::Simulation() : renderer(1024, 768) {
    if (!renderer.init()) {
        std::cerr << "Failed to initialize renderer.\n";
        exit(-1);
    }
    switchScene(0);
}

Simulation::~Simulation() {
    exporter.close();
}

void Simulation::switchScene(int index) {
    sceneIndex = 0;
    currentScene = std::make_unique<BlackHoleScene>(200);
    currentScene->init();
    
    // Black Hole: elevated angle view to see full accretion disk
    cameraPos = glm::vec3(0.0f, 12.0f, 25.0f);
    pitch = -25.0f;
    yaw = -90.0f;
    
    time = 0.0;
    firstMouse = true;
    
    std::cout << "Loaded Scene: " << currentScene->getName() << std::endl;
}

void Simulation::processInput() {
    GLFWwindow* window = renderer.window;
    
    // Exit
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }

    // Camera Direction based on pitch/yaw
    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(front);

    glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
    glm::vec3 up = glm::normalize(glm::cross(right, front));

    float cameraSpeed = 20.0f * dt;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) cameraSpeed *= 3.0f;

    // Movement
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
        cameraPos += cameraSpeed * front;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
        cameraPos -= cameraSpeed * front;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
        cameraPos -= cameraSpeed * right;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
        cameraPos += cameraSpeed * right;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        cameraPos += cameraSpeed * up;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        cameraPos -= cameraSpeed * up;

    // Mouse Look (Right-click drag)
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        
        if (firstMouse) {
            lastX = xpos;
            lastY = ypos;
            firstMouse = false;
        }

        float xoffset = xpos - lastX;
        float yoffset = lastY - ypos; // reversed since y-coordinates range from bottom to top
        lastX = xpos;
        lastY = ypos;

        float sensitivity = 0.2f;
        xoffset *= sensitivity;
        yoffset *= sensitivity;

        yaw += xoffset;
        pitch += yoffset;

        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;
    } else {
        firstMouse = true;
    }

    // Reset view
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
        cameraPos = glm::vec3(0.0f, 15.0f, 40.0f);
        pitch = -20.0f;
        yaw = -90.0f;
    }

    // Toggle pause (debounced)
    static bool pPressed = false;
    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
        if (!pPressed) {
            paused = !paused;
            pPressed = true;
            std::cout << (paused ? "Paused" : "Resumed") << "\n";
        }
    } else {
        pPressed = false;
    }

    // (Scene switching controls removed)

    // Data Export toggle
    static bool mPressed = false;
    if (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS) {
        if (!mPressed) {
            mPressed = true;
            static bool exporting = false;
            if (!exporting) {
                exporter.open("export.csv");
                exporting = true;
                std::cout << "Data Export Started: export.csv\n";
            } else {
                exporter.close();
                exporting = false;
                std::cout << "Data Export Stopped.\n";
            }
        }
    } else mPressed = false;
}

void Simulation::run() {
    std::cout << "Simulation Started!\n";
    std::cout << "Controls:\n";
    std::cout << " P: Pause/Resume\n";
    std::cout << " R: Reset Camera\n";
    std::cout << " WASD/QE: Fly Camera\n";
    std::cout << " Right-Click Drag: Look Around\n";
    std::cout << " M: Toggle Data Export to CSV\n";

    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(renderer.window)) {
        double currentTime = glfwGetTime();
        double frameTime = currentTime - lastTime;
        lastTime = currentTime;

        processInput();

        if (!paused) {
            currentScene->update(dt);
            time += dt;
            exporter.logState(time, currentScene->getBodies());
        }

        // Setup matrices
        glm::vec3 front;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front = glm::normalize(front);
        
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + front, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)renderer.width / (float)renderer.height, 0.1f, 1000.0f);

        renderer.render(currentScene.get(), view, projection);

        glfwPollEvents();
    }
}
