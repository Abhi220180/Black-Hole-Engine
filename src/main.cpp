#include "Simulation.hpp"
#include <iostream>

// Force Windows to use the NVIDIA discrete GPU instead of Intel integrated
extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement = 1;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

int main() {
    try {
        Simulation sim;
        sim.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return -1;
    }
    return 0;
}
