#pragma once

#include <random>
#include <cmath>

using namespace std;

class GeometricBrownianMotion {
private:
    double current_price;
    double drift;
    double volatility;
    double dt;
    mt19937 generator;
    normal_distribution<double> normal_dist;

public:
    GeometricBrownianMotion(double start_price, double mu, double sigma, double dt_step)
        : current_price(start_price), drift(mu), volatility(sigma), dt(dt_step),
          generator(random_device{}()), normal_dist(0.0, 1.0) {}

    double nextPrice() {
        double dW = normal_dist(generator) * sqrt(dt);
        current_price = current_price * exp((drift - 0.5 * volatility * volatility) * dt + volatility * dW);
        return current_price;
    }

    double getPrice() const { return current_price; }
};
