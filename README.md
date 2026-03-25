# Relativistic Black Hole Simulation Engine

A high-performance, GPU-accelerated C++ physics engine that merges interactive 3D rasterization with relativistic Schwarzschild black hole compute shaders.

## Showcase
*(Note: Replace the placeholder below with an animated GIF or screenshot of your simulation!)*
> **📸 Add your animated GIF or video here!** 
> *Show off the raymarched black hole, the trails, and the 3D spacetime grid.*

## Technical Highlights
- **Hybrid Rendering Pipeline:** Seamlessly composites massive parallel **OpenGL 4.3 Compute Shader** raymarching outputs with traditional, alpha-blended 3D rasterization grids and particle systems.
- **Relativistic Geodesics:** Simulates realistic light bending around a Schwarzschild metric using double-precision data streams.
- **Deeply Optimized Physics Integrator:** Features an extremely optimized $O(N^2)$ semi-implicit Euler N-body solver. Maximizes CPU cache-hits with register-level memory isolation and zeroes out per-frame O(N) heap allocations during the render loop.
- **Interactive 3D Fly Camera:** Free-roam WASD and Mouse look controls mapped dynamically globally.
- **Analytics & Exporting:** Natively streams and structures mathematical states out to CSV logs for external data analysis.

## Future Specifications & Roadmap
- **Machine Learning Integrations:** Inject neural networks for advanced state approximations and physics clustering.
- **Hierarchical Tree Optimizations (Barnes-Hut):** Expanding the spatial integration depth to support millions of bodies seamlessly in real-time.

## Build Instructions
1. Ensure your system has CMake (3.14+) installed and an active C++17 compiler (MSVC, GCC, or Clang).
2. Clone the repository:
   ```bash
   git clone <your-repo-link-here>
   cd "Relativistic Black Hole Simulation"
   ```
3. Generate and build the project using CMake. (Dependencies like `GLFW` and `GLM` are reliably fetched automatically through `FetchContent`).
   ```bash
   cmake -B build
   cmake --build build --config Release
   ```
4. Find the executable output within the `build` directory and run it!

## Credits & Acknowledgements
Initial OpenGL boilerplate, basic structure, and visual inspiration derived from the fantastic [kavan010's black_hole repository](https://github.com/kavan010/black_hole). 
This project builds significantly off that foundation by structurally isolating the logic into modular Classes (`Scene`, `Integrator`, `Renderer`), injecting advanced memory-access layout caching, supporting interactive fly-cameras, and hybridizing the specific compute-shader output.
