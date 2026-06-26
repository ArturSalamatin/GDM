#include "example_runner.h"
#include "../tests/simulation_cases/MultiLayerCase.h"
#include "../tests/well_schedule_builder.h"
#include "../tests/well_completion_builder.h"

namespace {

constexpr size_t Nx = 11, Ny = 11;
constexpr double Lx = 500.0, Ly = 500.0;
constexpr double hz = 10.0;

const std::vector<simulation_cases::WellInfo> seven_wells_info = {
    {"INJ-1",  "injector", 125.0, 125.0},
    {"INJ-2",  "injector", 375.0, 375.0},
    {"PROD-1", "producer", 375.0, 125.0},
    {"PROD-2", "producer", 125.0, 375.0},
    {"PROD-3", "producer", 250.0, 250.0},
    {"INJ-3",  "injector", 250.0, 125.0},
    {"PROD-4", "producer", 250.0, 375.0}
};

std::vector<test_helpers::WellScheduleBuilder>
make_7well_builders(size_t Nz, double hz_val, double total_time,
                    double rate_mult = 1.0)
{
    std::vector<test_helpers::WellScheduleBuilder> builders;

    auto c_inj1 = test_helpers::WellCompletionBuilder(Nz, hz_val)
        .open_layer(0, 0.0).open_layer(1, 0.0);
    builders.emplace_back(L"INJ-1", 125.0, 125.0);
    builders.back()
        .set_completions(c_inj1)
        .inject_water(40.0 * rate_mult).for_days(total_time);

    auto c_inj2 = test_helpers::WellCompletionBuilder(Nz, hz_val)
        .open_layer(0, 0.0).open_layer(2, 200.0);
    builders.emplace_back(L"INJ-2", 375.0, 375.0);
    builders.back()
        .set_completions(c_inj2)
        .inject_water(30.0 * rate_mult).for_days(total_time);

    auto c_prod1 = test_helpers::WellCompletionBuilder(Nz, hz_val)
        .open_layer(0, 0.0).open_layer(1, 0.0)
        .open_layer(2, 0.0).open_layer(3, 0.0);
    builders.emplace_back(L"PROD-1", 375.0, 125.0);
    builders.back()
        .set_completions(c_prod1)
        .produce_oil(25.0 * rate_mult).for_days(total_time);

    auto c_prod2 = test_helpers::WellCompletionBuilder(Nz, hz_val)
        .open_layer(3, 0.0);
    builders.emplace_back(L"PROD-2", 125.0, 375.0);
    builders.back()
        .set_completions(c_prod2)
        .produce_oil(15.0 * rate_mult).for_days(total_time);

    auto c_prod3 = test_helpers::WellCompletionBuilder(Nz, hz_val)
        .open_layer(1, 0.0).open_layer(2, 0.0)
        .close_layer(1, 300.0);
    builders.emplace_back(L"PROD-3", 250.0, 250.0);
    builders.back()
        .set_completions(c_prod3)
        .produce_oil(20.0 * rate_mult).for_days(total_time);

    auto c_inj3 = test_helpers::WellCompletionBuilder(Nz, hz_val)
        .open_layer(0, 150.0).open_layer(1, 150.0);
    builders.emplace_back(L"INJ-3", 250.0, 125.0);
    builders.back()
        .set_completions(c_inj3)
        .shut_in().for_days(150.0)
        .inject_water(35.0 * rate_mult).for_days(total_time - 150.0);

    auto c_prod4 = test_helpers::WellCompletionBuilder(Nz, hz_val)
        .open_layer(2, 300.0).open_layer(3, 300.0);
    builders.emplace_back(L"PROD-4", 250.0, 375.0);
    builders.back()
        .set_completions(c_prod4)
        .shut_in().for_days(300.0)
        .produce_oil(15.0 * rate_mult).for_days(total_time - 300.0);

    return builders;
}

void run_smoke() {
    constexpr size_t Nz = 4;
    constexpr double total_time = 600.0;

    simulation_cases::MultiLayerCase sc(
        "3d_smoke", Nx, Ny, Nz, Lx, Ly, hz,
        total_time, 30.0,
        [](double, double) { return make_7well_builders(4, hz, 600.0); },
        seven_wells_info
    );
    examples::run_case_3d(sc, "results");
}

void run_delayed_start() {
    constexpr size_t Nz = 2;

    simulation_cases::MultiLayerCase sc(
        "3d_delayed_start", Nx, Ny, Nz, Lx, Ly, hz,
        400.0, 20.0,
        [](double, double) {
            std::vector<test_helpers::WellScheduleBuilder> builders;

            auto c_inj = test_helpers::WellCompletionBuilder(Nz, hz)
                .open_layer(0, 0.0);
            builders.emplace_back(L"INJ", 125.0, 250.0);
            builders.back()
                .set_completions(c_inj)
                .inject_water(30.0).for_days(400.0);

            auto c_prod = test_helpers::WellCompletionBuilder(Nz, hz)
                .open_layer(0, 200.0);
            builders.emplace_back(L"PROD", 375.0, 250.0);
            builders.back()
                .set_completions(c_prod)
                .shut_in().for_days(200.0)
                .produce_oil(20.0).for_days(200.0);

            return builders;
        },
        {{"INJ",  "injector", 125.0, 250.0},
         {"PROD", "producer", 375.0, 250.0}}
    );
    examples::run_case_3d(sc, "results");
}

void run_layer_closure() {
    constexpr size_t Nz = 2;

    simulation_cases::MultiLayerCase sc(
        "3d_layer_closure", Nx, Ny, Nz, Lx, Ly, hz,
        400.0, 20.0,
        [](double, double) {
            std::vector<test_helpers::WellScheduleBuilder> builders;

            auto c_inj = test_helpers::WellCompletionBuilder(Nz, hz)
                .open_layer(0, 0.0).open_layer(1, 0.0)
                .close_layer(1, 200.0);
            builders.emplace_back(L"INJ", 250.0, 250.0);
            builders.back()
                .set_completions(c_inj)
                .inject_water(30.0).for_days(400.0);

            return builders;
        },
        {{"INJ", "injector", 250.0, 250.0}}
    );
    examples::run_case_3d(sc, "results");
}

} // namespace

int main() {
    run_smoke();
    run_delayed_start();
    run_layer_closure();
    return 0;
}
