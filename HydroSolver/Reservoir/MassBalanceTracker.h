#pragma once
#include <vector>

namespace reservoir_simulator {

class MassBalanceTracker {
public:
    void Initialize(double oilTotal, double waterTotal);

    void Update(double loc_tau,
                double oilTotal, double waterTotal,
                double oilContourFlux, double waterContourFlux,
                double oilDebitTotal, double waterDebitTotal);

    std::vector<double> GetBalance() const;

private:
    double prevOil_ = 0.0, curOil_ = 0.0;
    double accumOil_ = 0.0, accumOilOutFlux_ = 0.0, accumDebet_ = 0.0;
    double prevWater_ = 0.0, curWater_ = 0.0;
    double accumWater_ = 0.0, accumWaterOutFlux_ = 0.0, accumWaterDebet_ = 0.0;
    double curTime_ = 0.0;
};

} // namespace reservoir_simulator
