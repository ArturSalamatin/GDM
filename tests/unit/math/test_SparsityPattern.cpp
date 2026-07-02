#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Solver/Math/SparsityPattern.h"

using namespace reservoir_simulator::linear_problem;
using Catch::Approx;

TEST_CASE("SparsityPattern: default has empty Row and Col",
          "[unit][level2][math][SparsityPattern]") {
    SparsityPattern sp{};
    CHECK(sp.Row().empty());
    CHECK(sp.Col().empty());
}

TEST_CASE("SparsityPattern: 2-cell chain with full 2x2 block",
          "[unit][level2][math][SparsityPattern]") {
    std::vector<std::vector<int>> graph = {{1}, {0}};
    std::vector<bool> blockPattern = {true, true, true, true};
    SparsityPattern sp{2, 2, graph, blockPattern};

    CHECK(sp.EqNmbr() == 2);
    CHECK(sp.NmbrOfNonzerosPerUnitBlock() == 4);
    CHECK(sp.TotalNmbrOfBlocks() == 4);

    CHECK(sp.Row().size() == 5);
    auto& row = sp.Row();
    for (size_t i = 1; i < row.size(); i++)
        CHECK(row[i] >= row[i - 1]);
}

TEST_CASE("SparsityPattern: 3-cell chain produces 7 blocks",
          "[unit][level2][math][SparsityPattern]") {
    std::vector<std::vector<int>> graph = {{1}, {0, 2}, {1}};
    std::vector<bool> blockPattern = {true, true, true, true};
    SparsityPattern sp{2, 3, graph, blockPattern};

    CHECK(sp.TotalNmbrOfBlocks() == 7);
    CHECK(sp.Row().size() == 7);
}

TEST_CASE("SparsityPattern: DiagBlocks has correct size",
          "[unit][level2][math][SparsityPattern]") {
    std::vector<std::vector<int>> graph = {{1}, {0}};
    std::vector<bool> blockPattern = {true, true, true, true};
    SparsityPattern sp(2, 2, graph, blockPattern);

    CHECK(sp.DiagBlocks().size() == 8);
}

TEST_CASE("SparsityPattern: single cell has 1 block",
          "[unit][level2][math][SparsityPattern]") {
    std::vector<std::vector<int>> graph = {{}};
    std::vector<bool> blockPattern = {true, true, true, true};
    SparsityPattern sp{2, 1, graph, blockPattern};

    CHECK(sp.TotalNmbrOfBlocks() == 1);
    CHECK(sp.Row().size() == 3);
}

TEST_CASE("SparsityPattern: partial block pattern (diagonal only)",
          "[unit][level2][math][SparsityPattern]") {
    std::vector<std::vector<int>> graph = {{1}, {0}};
    std::vector<bool> blockPattern = {true, false, false, true};
    SparsityPattern sp{2, 2, graph, blockPattern};

    CHECK(sp.NmbrOfNonzerosPerUnitBlock() == 2);
    CHECK(sp.TotalNmbrOfBlocks() == 4);
    size_t expected_nnz = 2 * 4;
    CHECK(sp.Row().back() == expected_nnz);
}

TEST_CASE("SparsityPattern: 3-arg ctor blockSize equals eqNmbr squared",
          "[unit][level2][math][SparsityPattern]") {
    std::vector<std::vector<int>> graph = {{1}, {0}};
    SparsityPattern sp(2, 2, graph);

    CHECK(sp.NmbrOfNonzerosPerUnitBlock() == 4);
    CHECK(sp.TotalNmbrOfBlocks() == 4);
    CHECK(sp.Row().size() == 5);
}

TEST_CASE("SparsityPattern: 3-arg ctor matches 4-arg with full block",
          "[unit][level2][math][SparsityPattern]") {
    std::vector<std::vector<int>> graph = {{1, 2}, {0, 2}, {0, 1}};
    std::vector<bool> fullBlock = {true, true, true, true};
    SparsityPattern sp4(2, 3, graph, fullBlock);
    SparsityPattern sp3(2, 3, graph);

    CHECK(sp3.EqNmbr() == sp4.EqNmbr());
    CHECK(sp3.NmbrOfNonzerosPerUnitBlock() == sp4.NmbrOfNonzerosPerUnitBlock());
    CHECK(sp3.TotalNmbrOfBlocks() == sp4.TotalNmbrOfBlocks());
    CHECK(sp3.Row() == sp4.Row());
    CHECK(sp3.Col() == sp4.Col());
    CHECK(sp3.DiagBlocks() == sp4.DiagBlocks());
    CHECK(sp3.OffDiagBlocks() == sp4.OffDiagBlocks());
    CHECK(sp3.NmbrOfElementsAboveBlockRow() == sp4.NmbrOfElementsAboveBlockRow());
}

TEST_CASE("SparsityPattern: 3-arg ctor single cell eqNmbr 1",
          "[unit][level2][math][SparsityPattern]") {
    std::vector<std::vector<int>> graph = {{}};
    SparsityPattern sp(1, 1, graph);

    CHECK(sp.NmbrOfNonzerosPerUnitBlock() == 1);
    CHECK(sp.TotalNmbrOfBlocks() == 1);
    CHECK(sp.Row().size() == 2);
    CHECK(sp.Row()[0] == 0);
    CHECK(sp.Row()[1] == 1);
}
