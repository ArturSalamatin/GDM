#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Solver/Math/MatrixCSR.h"

using namespace reservoir_simulator::linear_problem;
using Catch::Approx;

static std::vector<std::vector<int>> twoCell_graph() {
    return {{1}, {0}};
}
static std::vector<bool> fullBlock2() {
    return {true, true, true, true};
}

TEST_CASE("MatrixCSR: default constructor",
          "[unit][level3][math][MatrixCSR]") {
    MatrixCSR m;
    CHECK(m.Val().empty());
}

TEST_CASE("MatrixCSR: construction allocates zero-filled values",
          "[unit][level3][math][MatrixCSR]") {
    MatrixCSR m(Layout::InterleavedSwP, 2, 2, twoCell_graph(), fullBlock2());
    CHECK(m.Val().size() > 0);
    for (double v : m.Val())
        CHECK(v == 0.0);
}

TEST_CASE("MatrixCSR: Row and Col are consistent with CRSStructure",
          "[unit][level3][math][MatrixCSR]") {
    MatrixCSR m(Layout::InterleavedSwP, 2, 2, twoCell_graph(), fullBlock2());
    CHECK(m.Row().size() == 5);
    CHECK(m.Col().size() == m.Row().back());
}

TEST_CASE("MatrixCSR: AddDiagBlock writes to correct positions",
          "[unit][level3][math][MatrixCSR]") {
    MatrixCSR m(Layout::InterleavedSwP, 2, 2, twoCell_graph(), fullBlock2());
    std::vector<double> block = {1.0, 2.0, 3.0, 4.0};
    m.AddDiagBlock(0, block);

    auto& diag = m.GetCRS().DiagBlocks();
    CHECK(m.Val()[diag[0]] == Approx(1.0));
    CHECK(m.Val()[diag[1]] == Approx(2.0));
    CHECK(m.Val()[diag[2]] == Approx(3.0));
    CHECK(m.Val()[diag[3]] == Approx(4.0));
}

TEST_CASE("MatrixCSR: AddDiagBlock with raw pointer",
          "[unit][level3][math][MatrixCSR]") {
    MatrixCSR m(Layout::InterleavedSwP, 2, 2, twoCell_graph(), fullBlock2());
    double block[] = {10.0, 20.0, 30.0, 40.0};
    m.AddDiagBlock(1, block);

    auto& diag = m.GetCRS().DiagBlocks();
    size_t offset = 1 * m.NmbrOfNonZerosPerUnitBlock();
    CHECK(m.Val()[diag[offset + 0]] == Approx(10.0));
    CHECK(m.Val()[diag[offset + 1]] == Approx(20.0));
}

TEST_CASE("MatrixCSR: ResetMatrix zeros all values",
          "[unit][level3][math][MatrixCSR]") {
    MatrixCSR m(Layout::InterleavedSwP, 2, 2, twoCell_graph(), fullBlock2());
    std::vector<double> block = {5.0, 6.0, 7.0, 8.0};
    m.AddDiagBlock(0, block);
    m.ResetMatrix();

    for (double v : m.Val())
        CHECK(v == 0.0);
}

TEST_CASE("MatrixCSR: AddDiagBlock accumulates",
          "[unit][level3][math][MatrixCSR]") {
    MatrixCSR m(Layout::InterleavedSwP, 2, 2, twoCell_graph(), fullBlock2());
    std::vector<double> block1 = {1.0, 0.0, 0.0, 1.0};
    std::vector<double> block2 = {2.0, 0.0, 0.0, 3.0};
    m.AddDiagBlock(0, block1);
    m.AddDiagBlock(0, block2);

    auto& diag = m.GetCRS().DiagBlocks();
    CHECK(m.Val()[diag[0]] == Approx(3.0));
    CHECK(m.Val()[diag[3]] == Approx(4.0));
}

TEST_CASE("MatrixCSR: NmbrOfNonZerosPerUnitBlock",
          "[unit][level3][math][MatrixCSR]") {
    MatrixCSR m(Layout::InterleavedSwP, 2, 2, twoCell_graph(), fullBlock2());
    CHECK(m.NmbrOfNonZerosPerUnitBlock() == 4);
}
