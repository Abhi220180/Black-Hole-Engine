#include "Exporter.hpp"

Exporter::~Exporter() {
    close();
}

void Exporter::open(const std::string& filename) {
    file.open(filename);
    if (file.is_open()) {
        file << "time,id,mass,x,y,z,vx,vy,vz\n";
        enabled = true;
    }
}

void Exporter::close() {
    if (file.is_open()) {
        file.close();
    }
    enabled = false;
}

void Exporter::logState(double time, const std::vector<Body>& bodies) {
    if (!enabled) return;
    
    for (const auto& b : bodies) {
        file << time << ","
             << b.id << ","
             << b.mass << ","
             << b.position.x << ","
             << b.position.y << ","
             << b.position.z << ","
             << b.velocity.x << ","
             << b.velocity.y << ","
             << b.velocity.z << "\n";
    }
}
