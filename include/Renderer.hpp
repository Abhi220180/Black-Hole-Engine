#pragma once
#include "glad/glad.h"
#include <GLFW/glfw3.h>
#include <vector>
#include <glm/glm.hpp>
#include "Scene.hpp"

class Renderer {
public:
    GLFWwindow* window;
    int width, height;

    Renderer(int width = 800, int height = 800);
    ~Renderer();

    bool init();
    void render(Scene* scene, const glm::mat4& view, const glm::mat4& projection);
    
private:
    GLuint shaderProgram;
    GLuint computeProgram;
    GLuint quadProgram;

    GLuint renderTexture;
    GLuint quadVAO, quadVBO;
    
    // UBOs for Compute Shader
    GLuint cameraUBO;
    GLuint diskUBO;
    GLuint objectsUBO;

    // For trails
    GLuint VAO, VBO;

    // For points (bodies)
    GLuint pointVAO, pointVBO;

    // For Grid (N-body scene)
    GLuint gridVAO, gridVBO;
    std::vector<float> gridVertices;
    int gridVertexCount;

    // Gravity Well Grid (Black Hole scene)
    GLuint gwGridVAO = 0, gwGridVBO = 0, gwGridEBO = 0;
    GLuint gwGridProgram = 0;
    int gwGridIndexCount = 0;

    void setupShaders();
    void setupComputeShader();
    void setupQuad();
    void setupGravityWellGrid();
    void generateGravityWellGrid(Scene* scene);
    void drawGravityWellGrid(const glm::mat4& view, const glm::mat4& projection);
    void updateComputeUBOs(Scene* scene, const glm::mat4& view, const glm::mat4& projection);
    void setupBuffers();
    void buildGrid();
    
    GLuint compileShader(GLenum type, const char* source);
};
