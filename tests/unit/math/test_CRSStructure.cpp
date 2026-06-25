#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Solver/Math/CRSStructure.h"

using namespace reservoir_simulator::linear_problem;
using Catch::Approx;

// 2 cells, neighbours: cell 0 <-> cell 1
// Full 2x2 block pattern
static std::vector<std::vector<int>> twoCell_graph() {
    return {{1}, {0}};
}
static std::vector<bool> fullBlock2() {
    return {true, true, true, true};
}

TEST_CASE("CRSStructure: InterleavedSwP basic properties",
          "[unit][level2][math][CRSStructure]") {
    CRSStructure crs(Layout::InterleavedSwP, 2, 2, twoCell_graph(), fullBlock2());
    CHECK(crs.EqNmbr() == 2);
    CHECK(crs.NnzPerBlock() == 4);
    CHECK(crs.GetLayout() == Layout::InterleavedSwP);
    CHECK(crs.TotalBlocks() == 4);
}

TEST_CASE("CRSStructure: InterleavedSwP Row has correct size",
          "[unit][level2][math][CRSStructure]") {
    CRSStructure crs(Layout::InterleavedSwP, 2, 2, twoCell_graph(), fullBlock2());
    CHECK(crs.Row().size() == 5);
}

TEST_CASE("CRSStructure: InterleavedSwP GlobalIndex identity",
          "[unit][level2][math][CRSStructure]") {
    CRSStructure crs(Layout::InterleavedSwP, 2, 2, twoCell_graph(), fullBlock2());
    CHECK(crs.GlobalIndex(0, 0) == 0);
    CHECK(crs.GlobalIndex(0, 1) == 1);
    CHECK(crs.GlobalIndex(1, 0) == 2);
    CHECK(crs.GlobalIndex(1, 1) == 3);
}

TEST_CASE("CRSStructure: InterleavedPSw swaps var order",
          "[unit][level2][math][CRSStructure]") {
    CRSStructure crs(Layout::InterleavedPSw, 2, 2, twoCell_graph(), fullBlock2());
    CHECK(crs.GlobalIndex(0, 0) == 1);
    CHECK(crs.GlobalIndex(0, 1) == 0);
    CHECK(crs.GlobalIndex(1, 0) == 3);
    CHECK(crs.GlobalIndex(1, 1) == 2);
}

TEST_CASE("CRSStructure: Blocked layout GlobalIndex",
          "[unit][level2][math][CRSStructure]") {
    CRSStructure crs(Layout::Blocked, 2, 2, twoCell_graph(), fullBlock2());
    CHECK(crs.GlobalIndex(0, 0) == 0);
    CHECK(crs.GlobalIndex(1, 0) == 1);
    CHECK(crs.GlobalIndex(0, 1) == 2);
    CHECK(crs.GlobalIndex(1, 1) == 3);
}

TEST_CASE("CRSStructure: UnpackCorrections extracts values for cell",
          "[unit][level2][math][CRSStructure]") {
    CRSStructure crs(Layout::InterleavedSwP, 2, 2, twoCell_graph(), fullBlock2());
    double corrections[] = {10.0, 20.0, 30.0, 40.0};
    double physical[2];

    crs.UnpackCorrections(0, corrections, physical);
    CHECK(physical[0] == Approx(10.0));
    CHECK(physical[1] == Approx(20.0));

    crs.UnpackCorrections(1, corrections, physical);
    CHECK(physical[0] == Approx(30.0));
    CHECK(physical[1] == Approx(40.0));
}

TEST_CASE("CRSStructure: 3-cell chain Row/Col consistency",
          "[unit][level2][math][CRSStructure]") {
    std::vector<std::vector<int>> graph = {{1}, {0, 2}, {1}};
    CRSStructure crs(Layout::InterleavedSwP, 2, 3, graph, fullBlock2());

    CHECK(crs.Row().size() == 7);
    CHECK(crs.TotalBlocks() == 7);

    auto& row = crs.Row();
    for (size_t i = 1; i < row.size(); i++)
        CHECK(row[i] >= row[i - 1]);
}
