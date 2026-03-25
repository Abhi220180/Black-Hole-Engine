cmake_minimum_required(VERSION 3.14)
project(SimulationEngine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# ----------
# Dependencies
# ----------
include(FetchContent)

# Fetch GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)


# Fetch GLM
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(glm)

# ----------
# Project Files
# ----------
set(SOURCES
    src/main.cpp
    src/glad.c
    src/Body.cpp
    src/Renderer.cpp
    src/Simulation.cpp
    src/Integrator.cpp
    src/BlackHoleScene.cpp
    src/Exporter.cpp
)

set(HEADERS
    include/Body.hpp
    include/Scene.hpp
    include/Renderer.hpp
    include/Simulation.hpp
    include/Integrator.hpp
    include/BlackHoleScene.hpp
    include/Exporter.hpp
)

# Main Executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    glfw
    glm::glm
)

# Copy shaders to the output directory so the executable can find them
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders
)


# Ensure folders exist in build context or copy assets
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/shaders DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/assets DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
