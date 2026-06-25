#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Solver/Math/MatrixCSR.h"
#include "assembly_test_helpers.h"

using namespace reservoir_simulator::linear_problem;
using namespace assembly_helpers;
using Catch::Approx;

// Smoke test: build 2×1 grid, add known blocks, verify dense matrix.
TEST_CASE("MatrixAssembly: 2x1x1 InterleavedSwP block placement",
          "[unit][level4][math][MatrixAssembly]") {
    auto graph = make_grid_graph(2, 1);
    MatrixCSR m(Layout::InterleavedSwP, 2, 2, graph, fullBlock2());

    std::array<double,4> d0 = {1, 2, 3, 4};
    std::array<double,4> d1 = {5, 6, 7, 8};
    std::array<double,4> off01 = {9, 10, 11, 12};
    std::array<double,4> off10 = {13, 14, 15, 16};

    m.AddDiagBlock(0, d0.data());
    m.AddDiagBlock(1, d1.data());
    std::vector<double> v01(off01.begin(), off01.end());
    std::vector<double> v10(off10.begin(), off10.end());
    m.AddOffDiagBlock(0, 0, v01);
    m.AddOffDiagBlock(1, 0, v10);

    auto dense = m.toDense();
    auto expected = build_dense_from_blocks(2, Layout::InterleavedSwP, graph,
        {{{0,0}, d0}, {{1,1}, d1}, {{0,1}, off01}, {{1,0}, off10}});

    check_dense_equal(dense, expected);
    check_zeros_are_zero(dense, m.Row(), m.Col());
    check_sparsity_symmetric(m);
    check_nnz_count(m, 2, graph);
}
