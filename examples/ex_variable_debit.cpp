#include "example_runner.h"
#include "../tests/simulation_cases/VariableDebitCase.h"

namespace {

constexpr size_t Nx = 21, Ny = 21;
constexpr double Lx = 250.0, Ly = 250.0;
constexpr double rho_oil = 800.0, rho_water = 1000.0;

void run_two_rates() {
    double Q1 = 30.0, Q2 = 60.0;
    double t1 = 90.0, t2 = 180.0;
    double hx = Lx / Nx, hy = Ly / Ny;
    double cx = (Nx / 2 + 0.5) * hx;
    double cy = (Ny / 2 + 0.5) * hy;

    simulation_cases::VariableDebitCase sc(
        "vardebit_two_rates", Nx, Ny, Lx, Ly,
        t1 + t2, 5.0,
        [&](double, double) {
            std::vector<test_helpers::WellScheduleBuilder> builders;
            builders.emplace_back(L"INJ", cx, cy);
            builders.back()
                .inject_water(Q1).for_days(t1)
                .inject_water(Q2).for_days(t2);
            return builders;
        },
        {{"INJ", "injector", cx, cy}}
    );
    examples::run_case(sc, "results");
}

void run_shut_in() {
    double Q = 50.0;
    double t_work = 60.0, t_shut = 60.0;
    double hx = Lx / Nx, hy = Ly / Ny;
    double cx = (Nx / 2 + 0.5) * hx;
    double cy = (Ny / 2 + 0.5) * hy;

    simulation_cases::VariableDebitCase sc(
        "vardebit_shut_in", Nx, Ny, Lx, Ly,
        t_work + t_shut + t_work, 5.0,
        [&](double, double) {
            std::vector<test_helpers::WellScheduleBuilder> builders;
            builders.emplace_back(L"INJ", cx, cy);
            builders.back()
                .inject_water(Q).for_days(t_work)
                .shut_in().for_days(t_shut)
                .inject_water(Q).for_days(t_work);
            return builders;
        },
        {{"INJ", "injector", cx, cy}}
    );
    examples::run_case(sc, "results");
}

void run_increasing_inj() {
    double Q_prod = 30.0;
    double Q_inj1 = 20.0, Q_inj2 = 40.0;
    double t1 = 120.0, t2 = 120.0;
    double hx = Lx / Nx, hy = Ly / Ny;
    double inj_x = (Nx / 4 + 0.5) * hx;
    double inj_y = (Ny / 2 + 0.5) * hy;
    double prod_x = (3 * Nx / 4 + 0.5) * hx;
    double prod_y = (Ny / 2 + 0.5) * hy;

    simulation_cases::VariableDebitCase sc(
        "vardebit_increasing_inj", Nx, Ny, Lx, Ly,
        t1 + t2, 5.0,
        [&](double, double) {
            std::vector<test_helpers::WellScheduleBuilder> builders;
            builders.emplace_back(L"INJ", inj_x, inj_y);
            builders.back()
                .inject_water(Q_inj1).for_days(t1)
                .inject_water(Q_inj2).for_days(t2);
            builders.emplace_back(L"PROD", prod_x, prod_y);
            builders.back()
                .produce_oil(Q_prod).for_days(t1 + t2);
            return builders;
        },
        {{"INJ", "injector", inj_x, inj_y},
         {"PROD", "producer", prod_x, prod_y}}
    );
    examples::run_case(sc, "results");
}

void run_alternating() {
    double Q = 40.0;
    double t_phase = 90.0;
    double hx = Lx / Nx, hy = Ly / Ny;
    double x_a = (Nx / 4 + 0.5) * hx;
    double x_b = (3 * Nx / 4 + 0.5) * hx;
    double cy = (Ny / 2 + 0.5) * hy;

    simulation_cases::VariableDebitCase sc(
        "vardebit_alternating", Nx, Ny, Lx, Ly,
        2 * t_phase, 5.0,
        [&](double, double) {
            std::vector<test_helpers::WellScheduleBuilder> builders;
            builders.emplace_back(L"INJ_A", x_a, cy);
            builders.back()
                .inject_water(Q).for_days(t_phase)
                .shut_in().for_days(t_phase);
            builders.emplace_back(L"INJ_B", x_b, cy);
            builders.back()
                .shut_in().for_days(t_phase)
                .inject_water(Q).for_days(t_phase);
            return builders;
        },
        {{"INJ_A", "injector", x_a, cy},
         {"INJ_B", "injector", x_b, cy}}
    );
    examples::run_case(sc, "results");
}

} // namespace

int main() {
    run_two_rates();
    run_shut_in();
    run_increasing_inj();
    run_alternating();
    return 0;
}
