#pragma once
#include <vector>
#include <array>
#include <map>
#include <cmath>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Solver/Math/MatrixCSR.h"
#include "Solver/Math/LinearProblem.h"
#include "Solver/Math/CRSStructure.h"

using namespace reservoir_simulator::linear_problem;

namespace assembly_helpers {

inline std::vector<bool> fullBlock2() { return {true, true, true, true}; }

// Build 2D connectivity graph for nx×ny grid (no Z-links).
// For 3D: call once per layer, offset indices by layer*nx*ny.
inline std::vector<std::vector<int>> make_grid_graph(int nx, int ny, int nz = 1)
{
    std::vector<std::vector<int>> graph;
    for (int k = 0; k < nz; k++) {
        int layer_offset = k * nx * ny;
        for (int j = 0; j < ny; j++) {
            for (int i = 0; i < nx; i++) {
                std::vector<int> connections;
                int local = layer_offset + j * nx + i;
                // Y- neighbour
                if (j > 0) connections.push_back(local - nx);
                // X- neighbour
                if (i > 0) connections.push_back(local - 1);
                // X+ neighbour
                if (i < nx - 1) connections.push_back(local + 1);
                // Y+ neighbour
                if (j < ny - 1) connections.push_back(local + nx);
                graph.push_back(connections);
            }
        }
    }
    return graph;
}

// Place a 2×2 physical block into dense matrix using Layout mapping.
// block layout: [row0_col0, row0_col1, row1_col0, row1_col1] in physical order.
// Physical: row 0 = Sw eq (oil), row 1 = P eq (water); col 0 = Sw, col 1 = P.
inline void place_block(
    std::vector<std::vector<double>>& dense,
    int cell_row, int cell_col,
    const std::array<double, 4>& block,
    Layout layout, size_t ncells)
{
    constexpr size_t B = 2;
    auto gi = [&](size_t cell, size_t var) -> size_t {
        if (layout == Layout::Blocked)
            return var * ncells + cell;
        else if (layout == Layout::InterleavedPSw) {
            unsigned char perm[] = {1, 0};
            return cell * B + perm[var];
        }
        else // InterleavedSwP
            return cell * B + var;
    };

    for (size_t r = 0; r < B; r++)
        for (size_t c = 0; c < B; c++)
            dense[gi(cell_row, r)][gi(cell_col, c)] += block[r * B + c];
}

// Build expected dense matrix from a map of (cell_row, cell_col) -> block.
inline std::vector<std::vector<double>> build_dense_from_blocks(
    size_t ncells, Layout layout,
    const std::vector<std::vector<int>>& graph,
    const std::map<std::pair<int,int>, std::array<double,4>>& blocks)
{
    constexpr size_t B = 2;
    size_t N = ncells * B;
    std::vector<std::vector<double>> dense(N, std::vector<double>(N, 0.0));
    for (auto& [key, block] : blocks)
        place_block(dense, key.first, key.second, block, layout, ncells);
    return dense;
}

// Compare two dense matrices element-by-element.
inline void check_dense_equal(
    const std::vector<std::vector<double>>& actual,
    const std::vector<std::vector<double>>& expected,
    double margin = 1e-12)
{
    REQUIRE(actual.size() == expected.size());
    for (size_t i = 0; i < actual.size(); i++) {
        REQUIRE(actual[i].size() == expected[i].size());
        for (size_t j = 0; j < actual[i].size(); j++) {
            INFO("mismatch at (" << i << ", " << j
                 << "): actual=" << actual[i][j]
                 << " expected=" << expected[i][j]);
            CHECK(actual[i][j] == Catch::Approx(expected[i][j]).margin(margin));
        }
    }
}

// Check that all elements outside the sparsity pattern are exactly zero.
inline void check_zeros_are_zero(
    const std::vector<std::vector<double>>& dense,
    const std::vector<size_t>& row,
    const std::vector<size_t>& col)
{
    size_t N = dense.size();
    // Build set of nonzero positions
    std::vector<std::vector<bool>> is_nonzero(N, std::vector<bool>(N, false));
    for (size_t i = 0; i < N; i++)
        for (size_t k = row[i]; k < row[i + 1]; k++)
            is_nonzero[i][col[k]] = true;

    for (size_t i = 0; i < N; i++)
        for (size_t j = 0; j < N; j++)
            if (!is_nonzero[i][j]) {
                INFO("expected zero at (" << i << ", " << j
                     << ") but got " << dense[i][j]);
                CHECK(dense[i][j] == 0.0);
            }
}

// Check sparsity pattern symmetry: if (i,j) is nonzero, then (j,i) is too.
inline void check_sparsity_symmetric(const MatrixCSR& m)
{
    auto& row = m.Row();
    auto& col = m.Col();
    size_t N = row.size() - 1;

    std::vector<std::vector<bool>> has(N, std::vector<bool>(N, false));
    for (size_t i = 0; i < N; i++)
        for (size_t k = row[i]; k < row[i + 1]; k++)
            has[i][col[k]] = true;

    for (size_t i = 0; i < N; i++)
        for (size_t j = 0; j < N; j++)
            if (has[i][j]) {
                INFO("sparsity asymmetry: (" << i << "," << j
                     << ") present but (" << j << "," << i << ") missing");
                CHECK(has[j][i]);
            }
}

// Check all diagonal elements are nonzero.
inline void check_nonzero_diagonal(const std::vector<std::vector<double>>& dense)
{
    for (size_t i = 0; i < dense.size(); i++) {
        INFO("zero diagonal at row " << i);
        CHECK(dense[i][i] != 0.0);
    }
}

// Check no NaN or Inf in a vector.
inline void check_finite(const std::vector<double>& v, const char* name)
{
    for (size_t i = 0; i < v.size(); i++) {
        INFO(name << "[" << i << "] is not finite: " << v[i]);
        CHECK(std::isfinite(v[i]));
    }
}

// Check expected number of nonzeros.
// For full 2×2 blocks: nnz = B² × (ncells + 2×edges)
inline void check_nnz_count(
    const MatrixCSR& m, size_t ncells,
    const std::vector<std::vector<int>>& graph)
{
    constexpr size_t B = 2;
    size_t edges = 0;
    for (auto& neib : graph)
        edges += neib.size();
    size_t expected_nnz = B * B * (ncells + edges);
    size_t actual_nnz = m.Row().back();
    INFO("nnz: actual=" << actual_nnz << " expected=" << expected_nnz);
    CHECK(actual_nnz == expected_nnz);
}

// Multiply dense matrix by vector: result = M * x
inline std::vector<double> matvec(
    const std::vector<std::vector<double>>& M,
    const std::vector<double>& x)
{
    size_t N = M.size();
    std::vector<double> result(N, 0.0);
    for (size_t i = 0; i < N; i++)
        for (size_t j = 0; j < N; j++)
            result[i] += M[i][j] * x[j];
    return result;
}

} // namespace assembly_helpers
