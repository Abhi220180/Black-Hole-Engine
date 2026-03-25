#pragma once
#include <string>
#include <vector>
#include <fstream>
#include "Body.hpp"

class Exporter {
private:
    std::ofstream file;
    bool enabled = false;

public:
    Exporter() = default;
    ~Exporter();

    void open(const std::string& filename);
    void close();
    void logState(double time, const std::vector<Body>& bodies);
};
