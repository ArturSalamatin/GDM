#pragma once

#include "test_helpers.h"
#include "well_completion_builder.h"
#include <cmath>
#include <stdexcept>
#include <string>

namespace test_helpers {

struct WellSchedule {
    reservoir_simulator::WellName name;
    reservoir_simulator::mer_descriptor::SingleWell_MER_Data mer_data;
    reservoir_simulator::WellJobsPerLayer jobs_per_layer;
    double x, y;
    double r_app;
};

class WellScheduleBuilder {
public:
    WellScheduleBuilder(const reservoir_simulator::WellName& name,
                        double x, double y)
        : name_(name), x_(x), y_(y) {}

    WellScheduleBuilder& inject_water(double vol_rate_m3_per_day) {
        pending_oil_mass_rate_ = 0.0;
        pending_water_mass_rate_ = -vol_rate_m3_per_day * rho_water_;
        return *this;
    }

    WellScheduleBuilder& produce_oil(double vol_rate_m3_per_day) {
        pending_oil_mass_rate_ = vol_rate_m3_per_day * rho_oil_;
        pending_water_mass_rate_ = 0.0;
        return *this;
    }

    WellScheduleBuilder& produce_liquid(double oil_vol, double water_vol) {
        pending_oil_mass_rate_ = oil_vol * rho_oil_;
        pending_water_mass_rate_ = water_vol * rho_water_;
        return *this;
    }

    WellScheduleBuilder& shut_in() {
        pending_oil_mass_rate_ = 0.0;
        pending_water_mass_rate_ = 0.0;
        is_shut_ = true;
        return *this;
    }

    WellScheduleBuilder& for_days(double days) {
        int n_records = static_cast<int>(std::ceil(days / month_));
        double remaining = days;

        for (int k = 0; k < n_records; ++k) {
            double dt = std::min(month_, remaining);
            remaining -= dt;

            reservoir_simulator::mer_descriptor::SingleMERrecord rec;
            rec["time"] = static_cast<float>(cursor_ + dt);
            rec["oil_m"] = static_cast<float>(pending_oil_mass_rate_ * dt);
            rec["water_m"] = static_cast<float>(pending_water_mass_rate_ * dt);
            rec["oil_v"] = static_cast<float>(pending_oil_mass_rate_ * dt / rho_oil_);
            rec["water_v"] = static_cast<float>(pending_water_mass_rate_ * dt / rho_water_);
            rec["type"] = 1.0f;
            rec["is_work"] = is_shut_ ? 0.0f : 1.0f;
            rec["worked_time"] = is_shut_ ? 0.0f : static_cast<float>(dt);
            rec["idle_time"] = is_shut_ ? static_cast<float>(dt) : 0.0f;
            rec["pump_water"] = 0.0f;

            mer_data_.push_back(rec);
            cursor_ += dt;
        }

        is_shut_ = false;
        return *this;
    }

    WellScheduleBuilder& set_r_app(double r) {
        r_app_ = r;
        return *this;
    }

    WellScheduleBuilder& set_hz(double hz) {
        hz_ = hz;
        return *this;
    }

    WellScheduleBuilder& set_completions(const WellCompletionBuilder& completions) {
        completions_ = completions.build();
        has_completions_ = true;
        return *this;
    }

    WellSchedule build() const {
        reservoir_simulator::WellJobsPerLayer jpl;
        if (has_completions_) {
            jpl = completions_;
        } else {
            reservoir_simulator::JobsInLayer jobs;
            jobs.emplace_back(0.0, hz_, true, 0.0);
            jpl.push_back(jobs);
        }

        return WellSchedule{
            name_, mer_data_, jpl, x_, y_, r_app_
        };
    }

    double total_days() const { return cursor_; }

    void add_to_sim(reservoir_simulator::ReservoirSimulator& sim,
                    const reservoir_simulator::DevelopedHorizon& horizon) const {
        auto schedule = build();

        double effective_r = schedule.r_app;
        if (effective_r <= 0.0)
            effective_r = 0.2 * horizon.block_size.step_x;

        reservoir_simulator::WellJobs well_jobs(schedule.name, schedule.jobs_per_layer);
        reservoir_simulator::WellPosition pos(schedule.x, schedule.y);

        sim.AddWell_FixedProduction(
            schedule.name, schedule.mer_data, well_jobs, pos,
            horizon.grid_bounds, horizon.block_size, effective_r);
    }

private:
    reservoir_simulator::WellName name_;
    double x_, y_;
    double r_app_ = 0.0;
    double hz_ = 10.0;

    reservoir_simulator::mer_descriptor::SingleWell_MER_Data mer_data_;
    double cursor_ = 0.0;

    double pending_oil_mass_rate_ = 0.0;
    double pending_water_mass_rate_ = 0.0;
    bool is_shut_ = false;

    reservoir_simulator::WellJobsPerLayer completions_;
    bool has_completions_ = false;

    static constexpr double month_ = 30.74;
    static constexpr double rho_oil_ = 800.0;
    static constexpr double rho_water_ = 1000.0;
};

} // namespace test_helpers
