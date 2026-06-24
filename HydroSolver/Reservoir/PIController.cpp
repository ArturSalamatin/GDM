#include "PIController.h"

namespace reservoir_simulator
{

double PIController::ComputeMultiplier(size_t newton_iters, bool success)
{
    double e_n;
    if (!success)
        e_n = 1.0;
    else
        e_n = static_cast<double>(newton_iters) / params_.max_iters;

    double e_target = static_cast<double>(params_.target_iters) / params_.max_iters;

    double mult;
    if (prev_error_ < 0) {
        mult = std::pow(e_target / e_n, params_.alpha);
    } else {
        mult = std::pow(e_target / e_n, params_.alpha)
             * std::pow(prev_error_ / e_n, params_.beta);
    }

    mult *= params_.safety;
    mult = std::clamp(mult, params_.min_shrink, params_.max_growth);
    prev_error_ = e_n;

    return mult;
}

void PIController::Reset()
{
    prev_error_ = -1.0;
}

} // namespace reservoir_simulator
