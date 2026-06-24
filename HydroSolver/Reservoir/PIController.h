#pragma once
#include <algorithm>
#include <cmath>

namespace reservoir_simulator
{

struct PIControllerParams {
    double alpha = 0.7;
    double beta = 0.2;
    size_t target_iters = 12;
    size_t max_iters = 65;
    double safety = 1.0;
    double max_growth = 2.0;
    double min_shrink = 0.3;
};

class PIController {
    PIControllerParams params_;
    double prev_error_ = -1.0;

public:
    explicit PIController(PIControllerParams p = {}) : params_(p) {}

    double ComputeMultiplier(size_t newton_iters, bool success);
    void Reset();
    const PIControllerParams& Params() const { return params_; }
};

} // namespace reservoir_simulator
