#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Solver/Math/MatrixCSR.h"
#include "assembly_test_helpers.h"

using namespace reservoir_simulator::linear_problem;
using namespace assembly_helpers;
using Catch::Approx;

// Fill matrix with distinct numbered blocks and verify against dense reference.
// Diag block for cell l: {l*10+1, l*10+2, l*10+3, l*10+4}
// OffDiag block (l→neib): {(l*100 + neib*10 + 1), ..., +4}
static void run_assembly_test(int nx, int ny, int nz, Layout layout)
{
    auto graph = make_grid_graph(nx, ny, nz);
    int ncells = static_cast<int>(graph.size());
    MatrixCSR m(layout, 2, ncells, graph, fullBlock2());

    std::map<std::pair<int,int>, std::array<double,4>> blocks;

    for (int l = 0; l < ncells; l++) {
        double base = (l + 1) * 10.0;
        std::array<double,4> diag = {base+1, base+2, base+3, base+4};
        m.AddDiagBlock(l, diag.data());
        blocks[{l, l}] = diag;

        for (int ni = 0; ni < static_cast<int>(graph[l].size()); ni++) {
            int neib = graph[l][ni];
            double obase = (l + 1) * 100.0 + (neib + 1) * 10.0;
            std::array<double,4> off = {obase+1, obase+2, obase+3, obase+4};
            std::vector<double> voff(off.begin(), off.end());
            m.AddOffDiagBlock(l, ni, voff);
            blocks[{l, neib}] = off;
        }
    }

    auto dense = m.toDense();
    auto expected = build_dense_from_blocks(ncells, layout, graph, blocks);

    check_dense_equal(dense, expected);
    check_zeros_are_zero(dense, m.Row(), m.Col());
    check_sparsity_symmetric(m);
    check_nnz_count(m, ncells, graph);
}

// --- 2x1x1 ---

TEST_CASE("MatrixAssembly: 2x1x1 InterleavedSwP",
          "[unit][level4][math][MatrixAssembly]") {
    run_assembly_test(2, 1, 1, Layout::InterleavedSwP);
}

TEST_CASE("MatrixAssembly: 2x1x1 InterleavedPSw",
          "[unit][level4][math][MatrixAssembly]") {
    run_assembly_test(2, 1, 1, Layout::InterleavedPSw);
}

TEST_CASE("MatrixAssembly: 2x1x1 Blocked",
          "[unit][level4][math][MatrixAssembly]") {
    run_assembly_test(2, 1, 1, Layout::Blocked);
}

// --- 3x1x1 ---

TEST_CASE("MatrixAssembly: 3x1x1 InterleavedSwP",
          "[unit][level4][math][MatrixAssembly]") {
    run_assembly_test(3, 1, 1, Layout::InterleavedSwP);
}

TEST_CASE("MatrixAssembly: 3x1x1 InterleavedPSw",
          "[unit][level4][math][MatrixAssembly]") {
    run_assembly_test(3, 1, 1, Layout::InterleavedPSw);
}

TEST_CASE("MatrixAssembly: 3x1x1 Blocked",
          "[unit][level4][math][MatrixAssembly]") {
    run_assembly_test(3, 1, 1, Layout::Blocked);
}

// --- 2x2x1 ---

TEST_CASE("MatrixAssembly: 2x2x1 InterleavedSwP",
          "[unit][level4][math][MatrixAssembly]") {
    run_assembly_test(2, 2, 1, Layout::InterleavedSwP);
}

TEST_CASE("MatrixAssembly: 2x2x1 InterleavedPSw",
          "[unit][level4][math][MatrixAssembly]") {
    run_assembly_test(2, 2, 1, Layout::InterleavedPSw);
}

TEST_CASE("MatrixAssembly: 2x2x1 Blocked",
          "[unit][level4][math][MatrixAssembly]") {
    run_assembly_test(2, 2, 1, Layout::Blocked);
}

// --- 3x3x1 ---

TEST_CASE("MatrixAssembly: 3x3x1 InterleavedSwP",
          "[unit][level4][math][MatrixAssembly]") {
    run_assembly_test(3, 3, 1, Layout::InterleavedSwP);
}

TEST_CASE("MatrixAssembly: 3x3x1 InterleavedPSw",
          "[unit][level4][math][MatrixAssembly]") {
    run_assembly_test(3, 3, 1, Layout::InterleavedPSw);
}

TEST_CASE("MatrixAssembly: 3x3x1 Blocked",
          "[unit][level4][math][MatrixAssembly]") {
    run_assembly_test(3, 3, 1, Layout::Blocked);
}

// --- 2x2x2 (two independent 2D layers, no Z-links) ---

TEST_CASE("MatrixAssembly: 2x2x2 InterleavedSwP",
          "[unit][level4][math][MatrixAssembly]") {
    run_assembly_test(2, 2, 2, Layout::InterleavedSwP);
}

TEST_CASE("MatrixAssembly: 2x2x2 InterleavedPSw",
          "[unit][level4][math][MatrixAssembly]") {
    run_assembly_test(2, 2, 2, Layout::InterleavedPSw);
}

TEST_CASE("MatrixAssembly: 2x2x2 Blocked",
          "[unit][level4][math][MatrixAssembly]") {
    run_assembly_test(2, 2, 2, Layout::Blocked);
}

// --- 3x3x3 (three independent 2D layers) ---

TEST_CASE("MatrixAssembly: 3x3x3 InterleavedSwP",
          "[unit][level4][math][MatrixAssembly]") {
    run_assembly_test(3, 3, 3, Layout::InterleavedSwP);
}

TEST_CASE("MatrixAssembly: 3x3x3 InterleavedPSw",
          "[unit][level4][math][MatrixAssembly]") {
    run_assembly_test(3, 3, 3, Layout::InterleavedPSw);
}

TEST_CASE("MatrixAssembly: 3x3x3 Blocked",
          "[unit][level4][math][MatrixAssembly]") {
    run_assembly_test(3, 3, 3, Layout::Blocked);
}

