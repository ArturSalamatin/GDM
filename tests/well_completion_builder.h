#pragma once

#include "test_helpers.h"
#include <stdexcept>

namespace test_helpers {

class WellCompletionBuilder {
public:
    WellCompletionBuilder(size_t nz, double hz)
        : nz_(nz), hz_(hz), jobs_(nz) {}

    WellCompletionBuilder& open_layer(size_t layer_id, double time) {
        jobs_.at(layer_id).emplace_back(0.0, hz_, true, time);
        return *this;
    }

    WellCompletionBuilder& close_layer(size_t layer_id, double time) {
        jobs_.at(layer_id).emplace_back(0.0, hz_, false, time);
        return *this;
    }

    WellCompletionBuilder& open_interval(size_t layer_id,
                                         double start, double end,
                                         double time) {
        jobs_.at(layer_id).emplace_back(start, end, true, time);
        return *this;
    }

    WellCompletionBuilder& close_interval(size_t layer_id,
                                          double start, double end,
                                          double time) {
        jobs_.at(layer_id).emplace_back(start, end, false, time);
        return *this;
    }

    reservoir_simulator::WellJobsPerLayer build() const {
        return jobs_;
    }

    size_t nz() const { return nz_; }
    double hz() const { return hz_; }

private:
    size_t nz_;
    double hz_;
    reservoir_simulator::WellJobsPerLayer jobs_;
};

} // namespace test_helpers
