cmake_minimum_required(VERSION 3.14)
project(SimulationEngine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CheckLanguage)
set(SIM_ENABLE_CUDA OFF)

set(CUDA_HOST_COMPILER_READY TRUE)
if(WIN32)
    if(CMAKE_GENERATOR MATCHES "Visual Studio")
        # Visual Studio generators provide the host compiler via toolset metadata.
        # Do not require cl.exe on PATH in this case.
    elseif(MSVC AND CMAKE_CXX_COMPILER)
        set(CMAKE_CUDA_HOST_COMPILER "${CMAKE_CXX_COMPILER}")
    else()
        find_program(MSVC_CL_EXECUTABLE cl.exe)
        if(MSVC_CL_EXECUTABLE)
            set(CMAKE_CUDA_HOST_COMPILER "${MSVC_CL_EXECUTABLE}")
        else()
            set(CUDA_HOST_COMPILER_READY FALSE)
            message(WARNING "MSVC host compiler was not found. CUDA backend disabled for this configure.")
        endif()
    endif()
endif()

if(CUDA_HOST_COMPILER_READY)
    if(NOT DEFINED CMAKE_CUDA_COMPILER AND NOT DEFINED ENV{CUDACXX})
        set(CUDA_NVCC_HINTS "")
        if(DEFINED ENV{CUDA_PATH})
            list(APPEND CUDA_NVCC_HINTS "$ENV{CUDA_PATH}/bin")
        endif()

        if(WIN32)
            file(GLOB CUDA_WIN_BIN_DIRS "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v*/bin")
            list(APPEND CUDA_NVCC_HINTS ${CUDA_WIN_BIN_DIRS})
        endif()

        find_program(CUDA_NVCC_EXECUTABLE nvcc HINTS ${CUDA_NVCC_HINTS})
        if(CUDA_NVCC_EXECUTABLE)
            set(CMAKE_CUDA_COMPILER "${CUDA_NVCC_EXECUTABLE}" CACHE FILEPATH "CUDA compiler" FORCE)
        endif()
    endif()

    check_language(CUDA)
    if(CMAKE_CUDA_COMPILER)
        enable_language(CUDA)
        find_package(CUDAToolkit QUIET)
        if(CUDAToolkit_FOUND)
            set(SIM_ENABLE_CUDA ON)
            message(STATUS "CUDA backend enabled.")
        else()
            message(WARNING "CUDA compiler found but CUDAToolkit was not found. Building without CUDA backend.")
        endif()
    else()
        message(STATUS "CUDA compiler not found. Building without CUDA backend.")
    endif()
endif()

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

if(SIM_ENABLE_CUDA)
    list(APPEND SOURCES
        src/CudaIntegrator.cpp
        src/CudaIntegrator.cu
    )
endif()

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

if(SIM_ENABLE_CUDA)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        CUDA_STANDARD 17
        CUDA_STANDARD_REQUIRED ON
        CUDA_SEPARABLE_COMPILATION ON
    )
endif()

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    glfw
    glm::glm
)

if(SIM_ENABLE_CUDA)
    target_link_libraries(${PROJECT_NAME} PRIVATE CUDA::cudart)
    target_compile_definitions(${PROJECT_NAME} PRIVATE SIM_ENABLE_CUDA=1)
endif()

# Copy shaders to the output directory so the executable can find them
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders
)


# Ensure folders exist in build context or copy assets
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/shaders DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/assets DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
