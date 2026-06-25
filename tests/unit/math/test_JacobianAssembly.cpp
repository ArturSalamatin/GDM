#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Reservoir/ReservoirSimulator.h"
#include "Solver/Grids/DevelopedHorizon.h"
#include "Data/PhaseFactory.hpp"
#include "Data/ExceptionFactory.h"
#include "assembly_test_helpers.h"
#include "../../test_helpers.h"

using namespace reservoir_simulator;
using namespace reservoir_simulator::linear_problem;
using namespace assembly_helpers;
using Catch::Approx;

static ReservoirSimulator make_sim(int nx, int ny, int nz, Layout layout)
{
    auto h = test_helpers::make_uniform_horizon(
        nx, ny, nz,
        100.0, 100.0, 10.0,
        100.0, 0.2,
        200.0, 0.8);
    auto np = test_helpers::default_num_params();
    return ReservoirSimulator(np, h, h.oil, h.water, h.other, layout);
}

// Reproduce fillMatrixBlockRow formulas, reading properties from real Grid cells.
// Returns {diagBlock[4], vector<offDiagBlock[4]>, rhsBlock[2]}.
static auto reference_fill_row(
    OilField& grid, size_t l, double tau)
    -> std::tuple<std::array<double,4>,
                  std::vector<std::array<double,4>>,
                  std::array<double,2>>
{
    const auto& cell = grid[l];
    auto neighbours = grid.GetNeighboursPointer(static_cast<int>(l));
    const auto& areas = grid.CommonEdgeArea(static_cast<int>(l));
    int nNeib = static_cast<int>(neighbours.size());

    std::array<double,4> blDiag = {};
    const auto prevMass = cell.PreviousState_Mass();
    std::array<double,2> rhs = {
        (prevMass[0] - cell.OilMass()) / tau,
        (prevMass[1] - cell.WaterMass()) / tau
    };

    blDiag[0] += cell.DerivativeMassOilBySwater() / tau;
    blDiag[2] += cell.DerivativeMassWaterBySwater() / tau;
    blDiag[1] += cell.DerivativeMassOilByP() / tau;
    blDiag[3] += cell.DerivativeMassWaterByP() / tau;

    double oilMobSum = 0.0, waterMobSum = 0.0;
    std::vector<std::array<double,4>> offDiags(nNeib, {0,0,0,0});

    for (int ni = 0; ni < nNeib; ni++) {
        const auto& neibCell = *neighbours[ni];
        double dp = cell.P() - neibCell.P();
        double p_grad = areas[ni] * dp;

        const TwoPhaseFlowCell* bwCell;
        std::array<double,4>* bwBlock;
        if (dp < 0.0) {
            bwCell = &neibCell;
            bwBlock = &offDiags[ni];
        } else {
            bwCell = &cell;
            bwBlock = &blDiag;
        }

        double mobCell = cell.MobilityOverall();
        double mobNeib = neibCell.MobilityOverall();
        double denom = mobCell + mobNeib;
        double meanMob = 2 * mobNeib * mobCell / denom;

        double c1 = 2 * std::pow(mobNeib / denom, 2)
            * (cell.DerivativeMobilityOil() + cell.DerivativeMobilityWater());
        double c3 = 2 * std::pow(mobCell / denom, 2)
            * (neibCell.DerivativeMobilityOil() + neibCell.DerivativeMobilityWater());

        double f_oil = bwCell->F_Oil();
        double f_water = bwCell->F_Water();

        double temp = p_grad * bwCell->Derivative_F_Oil() * meanMob;
        (*bwBlock)[0] += bwCell->DensityOil() * temp;
        (*bwBlock)[2] -= bwCell->DensityWater() * temp;

        double c_oil = -bwCell->DensityOil() * f_oil * meanMob * areas[ni];
        rhs[0] += c_oil * dp;
        offDiags[ni][1] += c_oil;
        double c_water = -bwCell->DensityWater() * f_water * meanMob * areas[ni];
        rhs[1] += c_water * dp;
        offDiags[ni][3] += c_water;
        oilMobSum += c_oil;
        waterMobSum += c_water;

        blDiag[0] += bwCell->DensityOil() * f_oil * c1 * p_grad;
        offDiags[ni][0] += bwCell->DensityOil() * f_oil * c3 * p_grad;
        blDiag[2] += bwCell->DensityWater() * f_water * c1 * p_grad;
        offDiags[ni][2] += bwCell->DensityWater() * f_water * c3 * p_grad;
    }

    blDiag[1] -= oilMobSum;
    blDiag[3] -= waterMobSum;

    return {blDiag, offDiags, rhs};
}

// Reproduce AccountForBoundaryConditions contribution for a boundary cell.
// dir: 0 = X-boundary (YZ face), 1 = Y-boundary (XZ face)
static auto reference_boundary_block(
    const TwoPhaseFlowCell& cell, double refPressure, int dir)
    -> std::pair<std::array<double,4>, std::array<double,2>>
{
    double hx = cell.StepX(), hy = cell.StepY(), hz = cell.StepZ();
    double faceArea;
    if (dir == 0)
        faceArea = hy * hz / (hx / 2);
    else
        faceArea = hx * hz / (hy / 2);

    double dp = cell.P() - refPressure;
    double p_grad = faceArea * dp;
    double curMob = cell.MobilityOverall();

    std::array<double,4> blDiag = {};
    std::array<double,2> rhs = {};

    double f_oil, f_water;
    if (dp > 0.0) {
        f_oil = cell.F_Oil() * cell.DensityOil();
        f_water = cell.F_Water() * cell.DensityWater();
        blDiag[0] += cell.DerivativeMobilityOil() * p_grad * cell.DensityOil();
        blDiag[2] += cell.DerivativeMobilityWater() * p_grad * cell.DensityWater();
    } else {
        f_oil = 0.0;
        f_water = 1.0 * cell.DensityWater(refPressure);
        blDiag[2] = (cell.DerivativeMobilityOil() + cell.DerivativeMobilityWater())
                     * p_grad * cell.DensityWater(refPressure);
    }

    blDiag[1] += faceArea * curMob * f_oil;
    blDiag[3] += faceArea * curMob * f_water;
    rhs[0] -= faceArea * curMob * f_oil * dp;
    rhs[1] -= faceArea * curMob * f_water * dp;

    return {blDiag, rhs};
}

// Build full reference dense matrix and RHS by iterating cells.
static void build_reference(
    ReservoirSimulator& sim, double tau, Layout layout,
    std::vector<std::vector<double>>& refDense,
    std::vector<double>& refRhs)
{
    OilField& grid = sim.Grid;
    int ncells = static_cast<int>(grid.ActiveCellsNmbr());
    int nx = static_cast<int>(grid.Nx());
    int ny = static_cast<int>(grid.Ny());
    int nz = static_cast<int>(grid.Nz());
    constexpr int B = 2;
    size_t N = ncells * B;

    refDense.assign(N, std::vector<double>(N, 0.0));
    refRhs.assign(N, 0.0);

    auto gi = [&](int cell, int var) -> size_t {
        if (layout == Layout::Blocked)
            return (size_t)var * ncells + cell;
        else if (layout == Layout::InterleavedPSw)
            return cell * B + (1 - var);
        else
            return cell * B + var;
    };

    auto add_diag = [&](int l, const std::array<double,4>& block, const std::array<double,2>& r) {
        for (int row = 0; row < B; row++)
            for (int col = 0; col < B; col++)
                refDense[gi(l, row)][gi(l, col)] += block[row * B + col];
        refRhs[gi(l, 0)] += r[0];
        refRhs[gi(l, 1)] += r[1];
    };

    auto add_offdiag = [&](int l, int neib, const std::array<double,4>& block) {
        for (int row = 0; row < B; row++)
            for (int col = 0; col < B; col++)
                refDense[gi(l, row)][gi(neib, col)] += block[row * B + col];
    };

    auto graph = grid.GetConnectivityGraph();

    for (int l = 0; l < ncells; l++) {
        auto [diag, offDiags, rhs] = reference_fill_row(grid, l, tau);
        add_diag(l, diag, rhs);
        for (int ni = 0; ni < static_cast<int>(offDiags.size()); ni++) {
            int neib = graph[l][ni];
            add_offdiag(l, neib, offDiags[ni]);
        }
    }

    // Boundary conditions
    double refP = sim.RefPressure;
    if (nx > 1) {
        for (int j = 0; j < ny; j++)
            for (int iStep = 0; iStep < 2; iStep++) {
                int i = (iStep == 0) ? 0 : nx - 1;
                for (int k = 0; k < nz; k++) {
                    size_t globalIdx = static_cast<size_t>(nx * ny * k + nx * j + i);
                    long int l = grid.ConvertGlobal2Local(globalIdx);
                    if (l < 0) continue;
                    auto [bd, br] = reference_boundary_block(grid[l], refP, 0);
                    add_diag(static_cast<int>(l), bd, br);
                }
            }
    }
    if (ny > 1) {
        for (int i = 0; i < nx; i++)
            for (int jStep = 0; jStep < 2; jStep++) {
                int j = (jStep == 0) ? 0 : ny - 1;
                for (int k = 0; k < nz; k++) {
                    size_t globalIdx = static_cast<size_t>(nx * ny * k + nx * j + i);
                    long int l = grid.ConvertGlobal2Local(globalIdx);
                    if (l < 0) continue;
                    auto [bd, br] = reference_boundary_block(grid[l], refP, 1);
                    add_diag(static_cast<int>(l), bd, br);
                }
            }
    }
}

// ============================================================
// Invariant tests
// ============================================================

static void run_invariant_test(int nx, int ny, int nz, Layout layout)
{
    auto sim = make_sim(nx, ny, nz, layout);
    double tau = 86400.0;
    sim.AssembleMyProblem(tau, tau);

    auto dense = sim.MyProblem.Matrix().toDense();
    auto& m = sim.MyProblem.Matrix();
    int ncells = static_cast<int>(sim.Grid.ActiveCellsNmbr());
    auto graph = sim.Grid.GetConnectivityGraph();

    check_sparsity_symmetric(m);
    check_nnz_count(m, ncells, graph);
    check_nonzero_diagonal(dense);
    check_finite(sim.MyProblem.Rhs(), "rhs");
}

TEST_CASE("JacobianAssembly: invariants 2x1x1 SwP",
          "[unit][level4][math][JacobianAssembly]") {
    run_invariant_test(2, 1, 1, Layout::InterleavedSwP);
}

TEST_CASE("JacobianAssembly: invariants 3x3x1 SwP",
          "[unit][level4][math][JacobianAssembly]") {
    run_invariant_test(3, 3, 1, Layout::InterleavedSwP);
}

TEST_CASE("JacobianAssembly: invariants 2x2x2 PSw",
          "[unit][level4][math][JacobianAssembly]") {
    run_invariant_test(2, 2, 2, Layout::InterleavedPSw);
}

TEST_CASE("JacobianAssembly: invariants 3x3x3 PSw",
          "[unit][level4][math][JacobianAssembly]") {
    run_invariant_test(3, 3, 3, Layout::InterleavedPSw);
}

TEST_CASE("JacobianAssembly: invariants 2x2x1 Blocked",
          "[unit][level4][math][JacobianAssembly]") {
    run_invariant_test(2, 2, 1, Layout::Blocked);
}

TEST_CASE("JacobianAssembly: invariants 3x3x3 Blocked",
          "[unit][level4][math][JacobianAssembly]") {
    run_invariant_test(3, 3, 3, Layout::Blocked);
}

// ============================================================
// RHS = 0 at initial state (prevMass = curMass, dp = 0, no wells)
// ============================================================

TEST_CASE("JacobianAssembly: RHS is zero at initial state",
          "[unit][level4][math][JacobianAssembly]") {
    auto sim = make_sim(3, 3, 1, Layout::InterleavedSwP);
    sim.AssembleMyProblem(86400.0, 86400.0);
    auto& rhs = sim.MyProblem.Rhs();
    for (size_t i = 0; i < rhs.size(); i++) {
        INFO("rhs[" << i << "] = " << rhs[i]);
        CHECK(rhs[i] == Approx(0.0).margin(1e-10));
    }
}

// ============================================================
// Element-wise comparison: real Jacobian vs reference formulas
// ============================================================

static void run_jacobian_test(int nx, int ny, int nz, Layout layout)
{
    auto sim = make_sim(nx, ny, nz, layout);
    double tau = 86400.0;
    sim.AssembleMyProblem(tau, tau);

    auto actualDense = sim.MyProblem.Matrix().toDense();
    auto& actualRhs = sim.MyProblem.Rhs();

    std::vector<std::vector<double>> refDense;
    std::vector<double> refRhs;
    build_reference(sim, tau, layout, refDense, refRhs);

    check_dense_equal(actualDense, refDense, 1e-10);

    REQUIRE(actualRhs.size() == refRhs.size());
    for (size_t i = 0; i < actualRhs.size(); i++) {
        INFO("rhs[" << i << "]: actual=" << actualRhs[i] << " expected=" << refRhs[i]);
        CHECK(actualRhs[i] == Approx(refRhs[i]).margin(1e-10));
    }
}

// --- InterleavedSwP ---

TEST_CASE("JacobianAssembly: 2x1x1 InterleavedSwP values",
          "[unit][level4][math][JacobianAssembly]") {
    run_jacobian_test(2, 1, 1, Layout::InterleavedSwP);
}

TEST_CASE("JacobianAssembly: 3x1x1 InterleavedSwP values",
          "[unit][level4][math][JacobianAssembly]") {
    run_jacobian_test(3, 1, 1, Layout::InterleavedSwP);
}

TEST_CASE("JacobianAssembly: 2x2x1 InterleavedSwP values",
          "[unit][level4][math][JacobianAssembly]") {
    run_jacobian_test(2, 2, 1, Layout::InterleavedSwP);
}

TEST_CASE("JacobianAssembly: 3x3x1 InterleavedSwP values",
          "[unit][level4][math][JacobianAssembly]") {
    run_jacobian_test(3, 3, 1, Layout::InterleavedSwP);
}

TEST_CASE("JacobianAssembly: 2x2x2 InterleavedSwP values",
          "[unit][level4][math][JacobianAssembly]") {
    run_jacobian_test(2, 2, 2, Layout::InterleavedSwP);
}

TEST_CASE("JacobianAssembly: 3x3x3 InterleavedSwP values",
          "[unit][level4][math][JacobianAssembly]") {
    run_jacobian_test(3, 3, 3, Layout::InterleavedSwP);
}

TEST_CASE("JacobianAssembly: 10x10x1 InterleavedSwP values",
          "[unit][level4][math][JacobianAssembly]") {
    run_jacobian_test(10, 10, 1, Layout::InterleavedSwP);
}

// --- InterleavedPSw ---

TEST_CASE("JacobianAssembly: 2x1x1 InterleavedPSw values",
          "[unit][level4][math][JacobianAssembly]") {
    run_jacobian_test(2, 1, 1, Layout::InterleavedPSw);
}

TEST_CASE("JacobianAssembly: 3x1x1 InterleavedPSw values",
          "[unit][level4][math][JacobianAssembly]") {
    run_jacobian_test(3, 1, 1, Layout::InterleavedPSw);
}

TEST_CASE("JacobianAssembly: 2x2x1 InterleavedPSw values",
          "[unit][level4][math][JacobianAssembly]") {
    run_jacobian_test(2, 2, 1, Layout::InterleavedPSw);
}

TEST_CASE("JacobianAssembly: 3x3x1 InterleavedPSw values",
          "[unit][level4][math][JacobianAssembly]") {
    run_jacobian_test(3, 3, 1, Layout::InterleavedPSw);
}

TEST_CASE("JacobianAssembly: 2x2x2 InterleavedPSw values",
          "[unit][level4][math][JacobianAssembly]") {
    run_jacobian_test(2, 2, 2, Layout::InterleavedPSw);
}

TEST_CASE("JacobianAssembly: 3x3x3 InterleavedPSw values",
          "[unit][level4][math][JacobianAssembly]") {
    run_jacobian_test(3, 3, 3, Layout::InterleavedPSw);
}

TEST_CASE("JacobianAssembly: 10x10x1 InterleavedPSw values",
          "[unit][level4][math][JacobianAssembly]") {
    run_jacobian_test(10, 10, 1, Layout::InterleavedPSw);
}

// --- Blocked ---

TEST_CASE("JacobianAssembly: 2x1x1 Blocked values",
          "[unit][level4][math][JacobianAssembly]") {
    run_jacobian_test(2, 1, 1, Layout::Blocked);
}

TEST_CASE("JacobianAssembly: 3x1x1 Blocked values",
          "[unit][level4][math][JacobianAssembly]") {
    run_jacobian_test(3, 1, 1, Layout::Blocked);
}

TEST_CASE("JacobianAssembly: 2x2x1 Blocked values",
          "[unit][level4][math][JacobianAssembly]") {
    run_jacobian_test(2, 2, 1, Layout::Blocked);
}

TEST_CASE("JacobianAssembly: 3x3x1 Blocked values",
          "[unit][level4][math][JacobianAssembly]") {
    run_jacobian_test(3, 3, 1, Layout::Blocked);
}

TEST_CASE("JacobianAssembly: 2x2x2 Blocked values",
          "[unit][level4][math][JacobianAssembly]") {
    run_jacobian_test(2, 2, 2, Layout::Blocked);
}

TEST_CASE("JacobianAssembly: 3x3x3 Blocked values",
          "[unit][level4][math][JacobianAssembly]") {
    run_jacobian_test(3, 3, 3, Layout::Blocked);
}

TEST_CASE("JacobianAssembly: 10x10x1 Blocked values",
          "[unit][level4][math][JacobianAssembly]") {
    run_jacobian_test(10, 10, 1, Layout::Blocked);
}

// ============================================================
// Uniform grid: identical interior cells -> identical blocks
// ============================================================

TEST_CASE("JacobianAssembly: uniform grid identical interior diag blocks",
          "[unit][level4][math][JacobianAssembly]") {
    auto sim = make_sim(5, 5, 1, Layout::InterleavedSwP);
    sim.AssembleMyProblem(86400.0, 86400.0);
    auto dense = sim.MyProblem.Matrix().toDense();

    // Interior cells: i in [1,3], j in [1,3] → all have 4 neighbours
    // Cell indices: l = j*5 + i for k=0
    std::vector<int> interior;
    for (int j = 1; j <= 3; j++)
        for (int i = 1; i <= 3; i++)
            interior.push_back(j * 5 + i);

    // All interior diag blocks should be identical
    auto get_diag = [&](int l) {
        int r0 = l * 2, r1 = l * 2 + 1;
        int c0 = l * 2, c1 = l * 2 + 1;
        return std::array<double,4>{dense[r0][c0], dense[r0][c1],
                                     dense[r1][c0], dense[r1][c1]};
    };
    auto ref = get_diag(interior[0]);
    for (size_t k = 1; k < interior.size(); k++) {
        auto cur = get_diag(interior[k]);
        for (int e = 0; e < 4; e++) {
            INFO("cell " << interior[k] << " diag[" << e << "]");
            CHECK(cur[e] == Approx(ref[e]).margin(1e-10));
        }
    }
}

// ============================================================
// J * dx ≈ ΔF (Jacobian consistency)
// ============================================================

// ============================================================
// Perturbed state: dp != 0 triggers upwind branching
// ============================================================

static void run_perturbed_jacobian_test(int nx, int ny, int nz, Layout layout)
{
    auto sim = make_sim(nx, ny, nz, layout);
    int ncells = static_cast<int>(sim.Grid.ActiveCellsNmbr());

    // Accept initial state as previous
    sim.Grid.AcceptState();

    // Perturb: create pressure gradient across x-direction
    int nx_grid = static_cast<int>(sim.Grid.Nx());
    for (int l = 0; l < ncells; l++) {
        int i = l % nx_grid;
        double dSw = 0.02 * (i - nx_grid / 2.0) / nx_grid;
        double dP = 1e5 * (i - nx_grid / 2.0) / nx_grid;
        double corr[] = {dSw, dP};
        sim.Grid[l].UpdateState(corr);
    }

    double tau = 86400.0;
    sim.AssembleMyProblem(tau, tau);

    auto actualDense = sim.MyProblem.Matrix().toDense();
    auto& actualRhs = sim.MyProblem.Rhs();

    std::vector<std::vector<double>> refDense;
    std::vector<double> refRhs;
    build_reference(sim, tau, layout, refDense, refRhs);

    check_dense_equal(actualDense, refDense, 1e-8);

    REQUIRE(actualRhs.size() == refRhs.size());
    for (size_t i = 0; i < actualRhs.size(); i++) {
        INFO("rhs[" << i << "]: actual=" << actualRhs[i] << " expected=" << refRhs[i]);
        CHECK(actualRhs[i] == Approx(refRhs[i]).margin(1e-8));
    }

    // RHS should NOT be zero anymore (state changed from previous)
    double rhs_norm = 0.0;
    for (auto v : actualRhs) rhs_norm += v * v;
    CHECK(rhs_norm > 1e-10);
}

TEST_CASE("JacobianAssembly: perturbed 3x3x1 InterleavedSwP",
          "[unit][level4][math][JacobianAssembly]") {
    run_perturbed_jacobian_test(3, 3, 1, Layout::InterleavedSwP);
}

TEST_CASE("JacobianAssembly: perturbed 3x3x1 InterleavedPSw",
          "[unit][level4][math][JacobianAssembly]") {
    run_perturbed_jacobian_test(3, 3, 1, Layout::InterleavedPSw);
}

TEST_CASE("JacobianAssembly: perturbed 3x3x1 Blocked",
          "[unit][level4][math][JacobianAssembly]") {
    run_perturbed_jacobian_test(3, 3, 1, Layout::Blocked);
}

TEST_CASE("JacobianAssembly: perturbed 5x5x1 InterleavedSwP",
          "[unit][level4][math][JacobianAssembly]") {
    run_perturbed_jacobian_test(5, 5, 1, Layout::InterleavedSwP);
}

TEST_CASE("JacobianAssembly: perturbed 2x2x2 InterleavedPSw",
          "[unit][level4][math][JacobianAssembly]") {
    run_perturbed_jacobian_test(2, 2, 2, Layout::InterleavedPSw);
}

// ============================================================
// J * dx ≈ ΔF (Jacobian consistency)
// ============================================================

TEST_CASE("JacobianAssembly: J*0 = -F = 0 at initial state",
          "[unit][level4][math][JacobianAssembly]") {
    auto sim = make_sim(3, 3, 1, Layout::InterleavedSwP);
    sim.AssembleMyProblem(86400.0, 86400.0);

    auto dense = sim.MyProblem.Matrix().toDense();
    size_t N = dense.size();
    std::vector<double> dx(N, 0.0);
    auto Jdx = matvec(dense, dx);

    for (size_t i = 0; i < N; i++) {
        INFO("J*0[" << i << "] = " << Jdx[i]);
        CHECK(Jdx[i] == Approx(0.0).margin(1e-15));
    }

    // Also: -F = 0 at initial state
    auto& rhs = sim.MyProblem.Rhs();
    for (size_t i = 0; i < N; i++) {
        INFO("-F[" << i << "] = " << rhs[i]);
        CHECK(rhs[i] == Approx(0.0).margin(1e-10));
    }
}
