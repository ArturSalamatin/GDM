#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include "Solver/Math/LinearProblem.h"
#include "assembly_test_helpers.h"

using namespace reservoir_simulator::linear_problem;
using namespace assembly_helpers;
using Catch::Approx;

// Fill LinearProblem with known diag+offdiag blocks and RHS,
// verify matrix toDense() == reference, RHS placement correct.
static void run_lp_test(int nx, int ny, int nz, Layout layout)
{
    auto graph = make_grid_graph(nx, ny, nz);
    int ncells = static_cast<int>(graph.size());
    constexpr int B = 2;

    LinearProblem lp(layout, 1e-6, 1e-6, graph, fullBlock2());

    std::map<std::pair<int,int>, std::array<double,4>> blocks;
    std::vector<double> expected_rhs(ncells * B, 0.0);

    auto gi = [&](int cell, int var) -> size_t {
        if (layout == Layout::Blocked)
            return (size_t)var * ncells + cell;
        else if (layout == Layout::InterleavedPSw) {
            unsigned char perm[] = {1, 0};
            return cell * B + perm[var];
        }
        else
            return cell * B + var;
    };

    for (int l = 0; l < ncells; l++) {
        double base = (l + 1) * 10.0;
        std::array<double,4> diag = {base+1, base+2, base+3, base+4};
        std::array<double,2> rhs = {base+5, base+6};

        lp.AddDiagBlock(l, diag.data(), rhs.data());
        blocks[{l, l}] = diag;

        expected_rhs[gi(l, 0)] += rhs[0];
        expected_rhs[gi(l, 1)] += rhs[1];

        for (int ni = 0; ni < static_cast<int>(graph[l].size()); ni++) {
            int neib = graph[l][ni];
            double obase = (l + 1) * 100.0 + (neib + 1) * 10.0;
            std::array<double,4> off = {obase+1, obase+2, obase+3, obase+4};

            lp.AddOffDiagBlock(l, ni, off.data());
            blocks[{l, neib}] = off;
        }
    }

    auto dense = lp.Matrix().toDense();
    auto expected_mat = build_dense_from_blocks(ncells, layout, graph, blocks);

    check_dense_equal(dense, expected_mat);
    check_zeros_are_zero(dense, lp.Matrix().Row(), lp.Matrix().Col());
    check_sparsity_symmetric(lp.Matrix());
    check_nnz_count(lp.Matrix(), ncells, graph);

    auto& rhs = lp.Rhs();
    REQUIRE(rhs.size() == expected_rhs.size());
    for (size_t i = 0; i < rhs.size(); i++) {
        INFO("rhs[" << i << "]: actual=" << rhs[i] << " expected=" << expected_rhs[i]);
        CHECK(rhs[i] == Approx(expected_rhs[i]));
    }
    check_finite(rhs, "rhs");
}

// --- 2x1x1 ---

TEST_CASE("LinearProblemAssembly: 2x1x1 InterleavedSwP",
          "[unit][level4][math][LinearProblemAssembly]") {
    run_lp_test(2, 1, 1, Layout::InterleavedSwP);
}

TEST_CASE("LinearProblemAssembly: 2x1x1 InterleavedPSw",
          "[unit][level4][math][LinearProblemAssembly]") {
    run_lp_test(2, 1, 1, Layout::InterleavedPSw);
}

TEST_CASE("LinearProblemAssembly: 2x1x1 Blocked",
          "[unit][level4][math][LinearProblemAssembly]") {
    run_lp_test(2, 1, 1, Layout::Blocked);
}

// --- 3x1x1 ---

TEST_CASE("LinearProblemAssembly: 3x1x1 InterleavedSwP",
          "[unit][level4][math][LinearProblemAssembly]") {
    run_lp_test(3, 1, 1, Layout::InterleavedSwP);
}

TEST_CASE("LinearProblemAssembly: 3x1x1 InterleavedPSw",
          "[unit][level4][math][LinearProblemAssembly]") {
    run_lp_test(3, 1, 1, Layout::InterleavedPSw);
}

TEST_CASE("LinearProblem::Solve returns finite error and converged=true",
          "[math][solver]") {
    auto graph = assembly_helpers::make_grid_graph(3, 1, 1);
    auto bp = assembly_helpers::fullBlock2();
    LinearProblem lp(Layout::InterleavedPSw, 1e-15, 1e-15, graph, bp);

    for (size_t i = 0; i < 3; ++i) {
        double diag[4] = {10.0, 0.1, 0.1, 10.0};
        double rhs[2] = {1.0, 0.5};
        lp.AddDiagBlock(i, diag, rhs);
    }
    for (size_t i = 0; i < 3; ++i) {
        for (int ni = 0; ni < static_cast<int>(graph[i].size()); ++ni) {
            double off[4] = {-1.0, 0.0, 0.0, -1.0};
            lp.AddOffDiagBlock(i, ni, off);
        }
    }

    auto res = lp.Solve(1);
    CHECK(res.converged);
    CHECK(std::isfinite(res.error));
}

TEST_CASE("LinearProblem::Solve converged=true when maxIter sufficient",
          "[math][solver]") {
    auto graph = assembly_helpers::make_grid_graph(3, 1, 1);
    auto bp = assembly_helpers::fullBlock2();
    LinearProblem lp(Layout::InterleavedPSw, 1e-2, 1e-2, graph, bp);

    for (size_t i = 0; i < 3; ++i) {
        double diag[4] = {10.0, 0.1, 0.1, 10.0};
        double rhs[2] = {1.0, 0.5};
        lp.AddDiagBlock(i, diag, rhs);
    }
    for (size_t i = 0; i < 3; ++i) {
        for (int ni = 0; ni < static_cast<int>(graph[i].size()); ++ni) {
            double off[4] = {-1.0, 0.0, 0.0, -1.0};
            lp.AddOffDiagBlock(i, ni, off);
        }
    }

    auto res = lp.Solve(100);
    CHECK(res.converged);
    CHECK(res.error <= 1e-2);
    CHECK(std::isfinite(res.error));
}

TEST_CASE("LinearProblemAssembly: 3x1x1 Blocked",
          "[unit][level4][math][LinearProblemAssembly]") {
    run_lp_test(3, 1, 1, Layout::Blocked);
}

// --- 2x2x1 ---

TEST_CASE("LinearProblemAssembly: 2x2x1 InterleavedSwP",
          "[unit][level4][math][LinearProblemAssembly]") {
    run_lp_test(2, 2, 1, Layout::InterleavedSwP);
}

TEST_CASE("LinearProblemAssembly: 2x2x1 InterleavedPSw",
          "[unit][level4][math][LinearProblemAssembly]") {
    run_lp_test(2, 2, 1, Layout::InterleavedPSw);
}

TEST_CASE("LinearProblemAssembly: 2x2x1 Blocked",
          "[unit][level4][math][LinearProblemAssembly]") {
    run_lp_test(2, 2, 1, Layout::Blocked);
}
