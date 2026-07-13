#include "MassBalanceTracker.h"

namespace reservoir_simulator {

void MassBalanceTracker::Initialize(double oilTotal, double waterTotal) {
    curOil_ = oilTotal;
    accumDebet_ = 0.0;
    accumOilOutFlux_ = 0.0;
    accumOil_ = 0.0;
    curWater_ = waterTotal;
    accumWaterDebet_ = 0.0;
    accumWaterOutFlux_ = 0.0;
    accumWater_ = 0.0;
    curTime_ = 0.0;
}

void MassBalanceTracker::Update(double loc_tau,
                                 double oilTotal, double waterTotal,
                                 double oilContourFlux, double waterContourFlux,
                                 double oilDebitTotal, double waterDebitTotal) {
    prevOil_ = curOil_;
    curOil_ = oilTotal;
    accumOil_ += curOil_ - prevOil_;
    accumOilOutFlux_ += oilContourFlux * loc_tau;
    accumDebet_ -= oilDebitTotal * loc_tau;

    prevWater_ = curWater_;
    curWater_ = waterTotal;
    accumWater_ += curWater_ - prevWater_;
    accumWaterOutFlux_ += waterContourFlux * loc_tau;
    accumWaterDebet_ -= waterDebitTotal * loc_tau;

    curTime_ += loc_tau;
}

std::vector<double> MassBalanceTracker::GetBalance() const {
    return {curTime_, accumOil_, accumOilOutFlux_, accumDebet_,
            accumWater_, accumWaterOutFlux_, accumWaterDebet_};
}

} // namespace reservoir_simulator
