#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Solver/Grids/Cells/TwoPhaseFlowCell.h"
#include <cmath>

using namespace reservoir_simulator::cell;
using Catch::Approx;

// ConstantPointProperties layout:
//   [0] mu_oil, [1] mu_water, [2] rho_oil, [3] rho_water,
//   [4] g, [5] c_oil, [6] c_water, [7] P_ref
static void init_constant_point_properties() {
    std::array<double, 8> props = {
        2.0,    // mu_oil (Pa*s or converted)
        1.0,    // mu_water
        800.0,  // rho_oil (kg/m3)
        1000.0, // rho_water
        9.81,   // g
        1e-9,   // c_oil (compressibility, 1/Pa)
        5e-10,  // c_water
        1e7     // P_ref (Pa)
    };
    PhysPropCell::set_constantPointProperties(props);
}

// constProp layout:
//   [0] permeability, [1] porosity, [2] So_res, [3] Sw_res,
//   [4..8] computed in constructor
// varProp: [0] Sw (physical), [1] P (Pa)
static TwoPhaseFlowCell make_cell(double Sw = 0.3, double P = 1e7,
                                   double perm = 1e-13, double poro = 0.2,
                                   double So_res = 0.1, double Sw_res = 0.2) {
    init_constant_point_properties();
    std::vector<double> center = {50.0, 50.0, 0.0};
    std::vector<double> size = {10.0, 10.0, 5.0};
    std::vector<double> constProp = {perm, poro, So_res, Sw_res,
                                      0, 0, 0, 0, 0};
    std::vector<double> varProp = {Sw, P};
    return TwoPhaseFlowCell(center, size, constProp, varProp);
}

TEST_CASE("TwoPhaseFlowCell: Corey relperm at Sw=Sw_res gives kro=1",
          "[unit][level3][physics][TwoPhaseFlowCell]") {
    auto cell = make_cell(0.2, 1e7);
    CHECK(cell.RelativePermeabilityWater() == Approx(0.0).margin(1e-12));
    CHECK(cell.RelativePermeabilityOil() == Approx(1.0));
}

TEST_CASE("TwoPhaseFlowCell: Corey relperm at Sw=1-So_res gives krw=1",
          "[unit][level3][physics][TwoPhaseFlowCell]") {
    auto cell = make_cell(0.9, 1e7);
    CHECK(cell.RelativePermeabilityWater() == Approx(1.0));
    CHECK(cell.RelativePermeabilityOil() == Approx(0.0).margin(1e-12));
}

TEST_CASE("TwoPhaseFlowCell: mobility sum = MobilityOverall",
          "[unit][level3][physics][TwoPhaseFlowCell]") {
    auto cell = make_cell(0.5, 1e7);
    CHECK(cell.MobilityOverall() == Approx(cell.MobilityOil() + cell.MobilityWater()));
}

TEST_CASE("TwoPhaseFlowCell: fractional flow sums to 1",
          "[unit][level3][physics][TwoPhaseFlowCell]") {
    auto cell = make_cell(0.5, 1e7);
    CHECK(cell.F_Oil() + cell.F_Water() == Approx(1.0));
}

TEST_CASE("TwoPhaseFlowCell: density depends on pressure through compressibility",
          "[unit][level3][physics][TwoPhaseFlowCell]") {
    init_constant_point_properties();
    auto cell_low = make_cell(0.5, 0.5e7);
    auto cell_high = make_cell(0.5, 2e7);
    CHECK(cell_high.DensityOil() > cell_low.DensityOil());
    CHECK(cell_high.DensityWater() > cell_low.DensityWater());
}

TEST_CASE("TwoPhaseFlowCell: SOil + SWater = 1 (physical saturations)",
          "[unit][level3][physics][TwoPhaseFlowCell]") {
    auto cell = make_cell(0.5, 1e7);
    CHECK(cell.SOil() + cell.SWater() == Approx(1.0));
}

TEST_CASE("TwoPhaseFlowCell: PoreVolume = porosity * Volume",
          "[unit][level3][physics][TwoPhaseFlowCell]") {
    auto cell = make_cell(0.5, 1e7, 1e-13, 0.25);
    CHECK(cell.PoreVolume() == Approx(0.25 * 500.0));
}

TEST_CASE("TwoPhaseFlowCell: AcceptState + ReverseState round-trip",
          "[unit][level3][physics][TwoPhaseFlowCell]") {
    auto cell = make_cell(0.5, 1e7);
    double sw_before = cell.SWater_Scaled();
    double p_before = cell.P();

    cell.AcceptState();
    double corrections[] = {0.1, 5e5};
    cell.UpdateState(corrections);
    CHECK(cell.SWater_Scaled() != Approx(sw_before));

    cell.ReverseState();
    CHECK(cell.SWater_Scaled() == Approx(sw_before));
    CHECK(cell.P() == Approx(p_before));
}

TEST_CASE("TwoPhaseFlowCell: OilMass and WaterMass are positive",
          "[unit][level3][physics][TwoPhaseFlowCell]") {
    auto cell = make_cell(0.5, 1e7);
    CHECK(cell.OilMass() > 0.0);
    CHECK(cell.WaterMass() > 0.0);
}
