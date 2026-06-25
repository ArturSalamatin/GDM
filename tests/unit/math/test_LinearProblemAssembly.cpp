#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Solver/Math/LinearProblem.h"
#include "assembly_test_helpers.h"

using namespace reservoir_simulator::linear_problem;
using namespace assembly_helpers;
using Catch::Approx;

TEST_CASE("LinearProblemAssembly: 2x1x1 InterleavedSwP matrix and rhs",
          "[unit][level4][math][LinearProblemAssembly]") {
    auto graph = make_grid_graph(2, 1);
    LinearProblem lp(Layout::InterleavedSwP, 1e-6, 1e-6, graph, fullBlock2());

    std::array<double,4> d0 = {1, 2, 3, 4};
    std::array<double,2> r0 = {10, 20};
    std::array<double,4> d1 = {5, 6, 7, 8};
    std::array<double,2> r1 = {30, 40};

    lp.AddDiagBlock(0, d0.data(), r0.data());
    lp.AddDiagBlock(1, d1.data(), r1.data());

    auto dense = lp.Matrix().toDense();
    auto expected = build_dense_from_blocks(2, Layout::InterleavedSwP, graph,
        {{{0,0}, d0}, {{1,1}, d1}});
    check_dense_equal(dense, expected);

    // RHS: GlobalIndex(cell, var) = cell*2 + var for InterleavedSwP
    auto& rhs = lp.Rhs();
    CHECK(rhs[0] == Approx(10.0));
    CHECK(rhs[1] == Approx(20.0));
    CHECK(rhs[2] == Approx(30.0));
    CHECK(rhs[3] == Approx(40.0));
    check_finite(rhs, "rhs");
}
