#pragma once
#include <string>
#include "Scene.hpp"

class BlackHoleScene : public Scene {
public:
    int numBodies = 200;
    double blackHoleMass = 1000.0;
    double eventHorizon = 1.0;

    BlackHoleScene(int numBodies = 200);

    void init() override;
    void update(double dt) override;
    void reset() override;
    
    std::string getName() const override { return "Black Hole Simulation"; }
};
