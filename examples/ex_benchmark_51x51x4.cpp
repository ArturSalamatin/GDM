#include "example_runner.h"
#include "../tests/simulation_cases/MultiLayerCase.h"
#include "../tests/well_schedule_builder.h"
#include "../tests/well_completion_builder.h"

namespace {

constexpr size_t Nx = 51, Ny = 51;
constexpr double Lx = 500.0, Ly = 500.0;
constexpr double hz = 10.0;
constexpr double total_time = 730.0;
constexpr double rate_mult = 1.4;

const std::vector<simulation_cases::WellInfo> wells_info = {
    {"INJ-1",  "injector", 125.0, 125.0},
    {"INJ-2",  "injector", 375.0, 375.0},
    {"PROD-1", "producer", 375.0, 125.0},
    {"PROD-2", "producer", 125.0, 375.0},
    {"PROD-3", "producer", 250.0, 250.0},
    {"INJ-3",  "injector", 250.0, 125.0}
};

std::vector<test_helpers::WellScheduleBuilder>
make_wells(size_t Nz, double hz_val)
{
    std::vector<test_helpers::WellScheduleBuilder> builders;

    auto c_inj1 = test_helpers::WellCompletionBuilder(Nz, hz_val)
        .open_layer(0, 0.0).open_layer(1, 0.0);
    builders.emplace_back("INJ-1", 125.0, 125.0);
    builders.back().set_completions(c_inj1)
        .inject_water(40.0 * rate_mult).for_days(total_time);

    auto c_inj2 = test_helpers::WellCompletionBuilder(Nz, hz_val)
        .open_layer(0, 0.0);
    builders.emplace_back("INJ-2", 375.0, 375.0);
    builders.back().set_completions(c_inj2)
        .inject_water(30.0 * rate_mult).for_days(total_time);

    auto c_prod1 = test_helpers::WellCompletionBuilder(Nz, hz_val)
        .open_layer(0, 0.0).open_layer(1, 0.0)
        .open_layer(2, 0.0).open_layer(3, 0.0);
    builders.emplace_back("PROD-1", 375.0, 125.0);
    builders.back().set_completions(c_prod1)
        .produce_oil(25.0 * rate_mult).for_days(total_time);

    auto c_prod2 = test_helpers::WellCompletionBuilder(Nz, hz_val)
        .open_layer(3, 0.0);
    builders.emplace_back("PROD-2", 125.0, 375.0);
    builders.back().set_completions(c_prod2)
        .produce_oil(15.0 * rate_mult).for_days(total_time);

    auto c_prod3 = test_helpers::WellCompletionBuilder(Nz, hz_val)
        .open_layer(1, 0.0).open_layer(2, 0.0);
    builders.emplace_back("PROD-3", 250.0, 250.0);
    builders.back().set_completions(c_prod3)
        .produce_oil(20.0 * rate_mult).for_days(total_time);

    auto c_inj3 = test_helpers::WellCompletionBuilder(Nz, hz_val)
        .open_layer(0, 150.0).open_layer(1, 150.0);
    builders.emplace_back("INJ-3", 250.0, 125.0);
    builders.back().set_completions(c_inj3)
        .shut_in().for_days(150.0)
        .inject_water(35.0 * rate_mult).for_days(total_time - 150.0);

    return builders;
}

} // namespace

int main() {
    constexpr size_t Nz = 4;
    constexpr double snapshot_dt = 5.0;

    simulation_cases::MultiLayerCase sc(
        "benchmark_51x51x4", Nx, Ny, Nz, Lx, Ly, hz,
        total_time, snapshot_dt,
        [](double, double) { return make_wells(4, hz); },
        wells_info
    );
    examples::run_case_3d(sc, "results");
    return 0;
}
