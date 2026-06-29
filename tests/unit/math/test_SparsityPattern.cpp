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
