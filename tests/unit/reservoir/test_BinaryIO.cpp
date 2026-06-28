#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <filesystem>
#include <fstream>
#include <array>
#include <vector>
#include <cstdint>
#include <tuple>

TEST_CASE("BUG-012: size_t write vs int read produces wrong grid_dim", "[binary-io][bug-012]") {
    namespace fs = std::filesystem;
    auto tmpFile = fs::temp_directory_path() / "bug012_test.bin";

    {
        std::ofstream out(tmpFile, std::ios::binary);
        double saturation_date = 0.0, startDate = 0.0, endDate = 100.0;
        int frameCount = 1;
        out.write(reinterpret_cast<const char*>(&saturation_date), sizeof(double));
        out.write(reinterpret_cast<const char*>(&startDate), sizeof(double));
        out.write(reinterpret_cast<const char*>(&endDate), sizeof(double));
        out.write(reinterpret_cast<const char*>(&frameCount), sizeof(int));

        std::vector<size_t> grid_dim{ 51, 51, 4 };
        out.write(reinterpret_cast<const char*>(grid_dim.data()), grid_dim.size() * sizeof(size_t));
    }

    {
        std::ifstream in(tmpFile, std::ios::binary);
        double saturation_date, startDate, endDate;
        int frameCount;
        in.read(reinterpret_cast<char*>(&saturation_date), sizeof(double));
        in.read(reinterpret_cast<char*>(&startDate), sizeof(double));
        in.read(reinterpret_cast<char*>(&endDate), sizeof(double));
        in.read(reinterpret_cast<char*>(&frameCount), sizeof(int));

        std::array<int, 3> grid_dim_int;
        in.read(reinterpret_cast<char*>(&grid_dim_int), 3 * sizeof(int));

        // Documents the bug: int read misinterprets size_t data
        CHECK(grid_dim_int[0] == 51);   // low 32 bits of Nx — coincidentally correct
        CHECK(grid_dim_int[1] == 0);    // high 32 bits of Nx — NOT Ny!
        CHECK(grid_dim_int[2] == 51);   // low 32 bits of Ny — NOT Nz!
    }

    fs::remove(tmpFile);
}

TEST_CASE("BUG-012 fix: size_t round-trip preserves grid_dim", "[binary-io][bug-012]") {
    namespace fs = std::filesystem;
    auto tmpFile = fs::temp_directory_path() / "bug012_fix_test.bin";

    const size_t expectedNx = 51, expectedNy = 51, expectedNz = 4;

    {
        std::ofstream out(tmpFile, std::ios::binary);
        double saturation_date = 0.0, startDate = 0.0, endDate = 100.0;
        int frameCount = 1;
        out.write(reinterpret_cast<const char*>(&saturation_date), sizeof(double));
        out.write(reinterpret_cast<const char*>(&startDate), sizeof(double));
        out.write(reinterpret_cast<const char*>(&endDate), sizeof(double));
        out.write(reinterpret_cast<const char*>(&frameCount), sizeof(int));

        std::vector<size_t> grid_dim{ expectedNx, expectedNy, expectedNz };
        out.write(reinterpret_cast<const char*>(grid_dim.data()), grid_dim.size() * sizeof(size_t));
    }

    {
        std::ifstream in(tmpFile, std::ios::binary);
        double saturation_date, startDate, endDate;
        int frameCount;
        in.read(reinterpret_cast<char*>(&saturation_date), sizeof(double));
        in.read(reinterpret_cast<char*>(&startDate), sizeof(double));
        in.read(reinterpret_cast<char*>(&endDate), sizeof(double));
        in.read(reinterpret_cast<char*>(&frameCount), sizeof(int));

        std::array<size_t, 3> grid_dim;
        in.read(reinterpret_cast<char*>(grid_dim.data()), 3 * sizeof(size_t));

        REQUIRE(grid_dim[0] == expectedNx);
        REQUIRE(grid_dim[1] == expectedNy);
        REQUIRE(grid_dim[2] == expectedNz);
    }

    fs::remove(tmpFile);
}

TEST_CASE("Binary header round-trip: various grid sizes", "[binary-io]") {
    namespace fs = std::filesystem;
    auto tmpFile = fs::temp_directory_path() / "binary_header_roundtrip.bin";

    auto [nx, ny, nz] = GENERATE(
        std::tuple{size_t(1), size_t(1), size_t(1)},
        std::tuple{size_t(51), size_t(51), size_t(4)},
        std::tuple{size_t(100), size_t(200), size_t(8)},
        std::tuple{size_t(1000), size_t(1000), size_t(1)}
    );

    const double expectedEnd = 365.25;
    const int expectedFrames = 42;

    {
        std::ofstream out(tmpFile, std::ios::binary);
        double sd = 0.0, st = 0.0;
        out.write(reinterpret_cast<const char*>(&sd), sizeof(double));
        out.write(reinterpret_cast<const char*>(&st), sizeof(double));
        out.write(reinterpret_cast<const char*>(&expectedEnd), sizeof(double));
        out.write(reinterpret_cast<const char*>(&expectedFrames), sizeof(int));
        std::array<size_t, 3> dims{nx, ny, nz};
        out.write(reinterpret_cast<const char*>(dims.data()), 3 * sizeof(size_t));
        int sentinel = 0xDEAD;
        out.write(reinterpret_cast<const char*>(&sentinel), sizeof(int));
    }

    {
        std::ifstream in(tmpFile, std::ios::binary);
        double sd, st, endDate;
        int frameCount;
        in.read(reinterpret_cast<char*>(&sd), sizeof(double));
        in.read(reinterpret_cast<char*>(&st), sizeof(double));
        in.read(reinterpret_cast<char*>(&endDate), sizeof(double));
        in.read(reinterpret_cast<char*>(&frameCount), sizeof(int));

        std::array<size_t, 3> grid_dim;
        in.read(reinterpret_cast<char*>(grid_dim.data()), 3 * sizeof(size_t));

        REQUIRE(grid_dim[0] == nx);
        REQUIRE(grid_dim[1] == ny);
        REQUIRE(grid_dim[2] == nz);
        REQUIRE(endDate == expectedEnd);
        REQUIRE(frameCount == expectedFrames);

        int sentinel;
        in.read(reinterpret_cast<char*>(&sentinel), sizeof(int));
        REQUIRE(sentinel == 0xDEAD);
    }

    fs::remove(tmpFile);
}
