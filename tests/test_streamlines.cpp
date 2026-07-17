#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <fstream>
#include <filesystem>
#include <cmath>

#include "simulation_cases/SingleInjectorCase.h"
#include "simulation_cases/TwoWellCase.h"

#include "../HydroSolver/Anomaly/FlowField/FlowField.h"

namespace fs = std::filesystem;
using namespace reservoir_simulator;
using namespace reservoir_simulator::phasePortrait;

// ─── helpers ───────────────────────────────────────────────────────────────────

static std::unique_ptr<ReservoirSimulator> run_simulation(
    const simulation_cases::SimulationCase& sc,
    double max_time = -1.0)
{
    auto horizon = sc.make_horizon();
    auto numPrm  = sc.make_num_params();

    auto sim = std::make_unique<ReservoirSimulator>(
        numPrm, horizon, horizon.oil, horizon.water, horizon.other);
    sim->RefPressure = sc.ref_pressure_Pa();
    sim->numPrm.set_initial_schemeTau(sc.initial_tau());
    sim->numPrm.set_currentMoment(0.0);
    sc.add_wells(*sim, horizon);

    auto times = sc.save_times();
    for (size_t step = 1; step < times.size(); ++step) {
        if (max_time > 0.0 && times[step] > max_time) break;
        sim->Solve({times[step - 1], times[step]});
    }

    if (max_time > 0.0 && max_time > times.back()) {
        double dt_extend = times.back() > times.front()
            ? times[1] - times[0] : 10.0;
        double t_cur = times.back();
        while (t_cur < max_time) {
            double t_next = std::min(t_cur + dt_extend, max_time);
            sim->Solve({t_cur, t_next});
            t_cur = t_next;
        }
    }

    return sim;
}

static std::vector<Point> make_radial_starts(double cx, double cy, double r, int count)
{
    constexpr double PI = 3.141592653589793;
    std::vector<Point> pts;
    double dPhi = 2 * PI / count;
    for (int i = 0; i < count; ++i)
        pts.emplace_back(cx + r * cos(i * dPhi), cy + r * sin(i * dPhi));
    return pts;
}

static void export_streamlines_csv(
    const std::string& path,
    const PhasePortrait& portrait)
{
    std::ofstream ofs(path);
    ofs << "trajectory_id,point_id,t,x,y\n";
    for (size_t i = 0; i < portrait.NumberOfTrajectories(); ++i)
        for (size_t j = 0; j < portrait[static_cast<int>(i)].size(); ++j)
            ofs << i << "," << j << ","
                << portrait[static_cast<int>(i)][j].t() << ","
                << portrait[static_cast<int>(i)][j].x() << "," << portrait[static_cast<int>(i)][j].y() << "\n";
}

static void export_velocity_csv(
    const std::string& path,
    const FlowFieldSnapshot& snap,
    size_t nx, size_t ny)
{
    const auto& vx_mesh_x = snap.get_vxField().x_mesh();
    const auto& vx_mesh_y = snap.get_vxField().y_mesh();
    const auto& vx_data   = snap.get_vxField().get_field();

    const auto& vy_mesh_x = snap.get_vyField().x_mesh();
    const auto& vy_mesh_y = snap.get_vyField().y_mesh();
    const auto& vy_data   = snap.get_vyField().get_field();

    std::ofstream ofs(path);
    ofs << "x,y,vx,vy\n";

    double hx = vx_mesh_x.size() > 1 ? vx_mesh_x[1] - vx_mesh_x[0] : 1.0;
    double hy = vy_mesh_y.size() > 1 ? vy_mesh_y[1] - vy_mesh_y[0] : 1.0;
    double x0 = vx_mesh_x[0] + hx * 0.5;
    double y0 = vy_mesh_y[0] + hy * 0.5;

    for (size_t j = 0; j < ny; ++j)
        for (size_t i = 0; i < nx; ++i) {
            double cx = x0 + i * hx;
            double cy = y0 + j * hy;
            double vx_val = (vx_data[j][i] + vx_data[j][i + 1]) * 0.5;
            double vy_val = (vy_data[j][i] + vy_data[j + 1][i]) * 0.5;
            ofs << cx << "," << cy << "," << vx_val << "," << vy_val << "\n";
        }
}

#ifdef NDEBUG
constexpr size_t SL_N = 21;
constexpr double SL_sim_end = 1000.0;
constexpr int SL_n_rays = 24;
#else
constexpr size_t SL_N = 11;
constexpr double SL_sim_end = 100.0;
constexpr int SL_n_rays = 8;
#endif

// ─── single injector ───────────────────────────────────────────────────────────

TEST_CASE("Streamlines: single injector radial",
          "[streamlines][single-injector]")
{
    constexpr double sim_end = SL_sim_end;
    simulation_cases::SingleInjectorCase sc(SL_N, SL_N);
    auto sim = run_simulation(sc, sim_end);

    auto& field = sim->flowFields[0];
    REQUIRE_FALSE(field.is_empty());

    double last_time = sim_end;

    auto wells = sc.wells_info();
    double cx = wells[0].x, cy = wells[0].y;
    double hx = sc.lx() / sc.nx();
    double r  = 1.0 * hx;
    int n_rays = SL_n_rays;

    auto starts = make_radial_starts(cx, cy, r, n_rays);
    PhasePortrait portrait;
    portrait.InstansiateTrajectories(starts, 0.0);
    portrait.FollowTrajectories(last_time, field, 1.0);

    SECTION("trajectories are not empty") {
        for (size_t i = 0; i < portrait.NumberOfTrajectories(); ++i) {
            INFO("trajectory " << i);
            REQUIRE(portrait[static_cast<int>(i)].size() >= 2);
        }
    }

    SECTION("all points are finite and within domain") {
        for (size_t i = 0; i < portrait.NumberOfTrajectories(); ++i)
            for (size_t j = 0; j < portrait[static_cast<int>(i)].size(); ++j) {
                INFO("traj=" << i << " pt=" << j);
                REQUIRE(std::isfinite(portrait[static_cast<int>(i)][j].x()));
                REQUIRE(std::isfinite(portrait[static_cast<int>(i)][j].y()));
            }
    }

    SECTION("trajectories move outward from injector") {
        for (size_t i = 0; i < portrait.NumberOfTrajectories(); ++i) {
            double r_start = std::hypot(portrait[static_cast<int>(i)][0].x() - cx, portrait[static_cast<int>(i)][0].y() - cy);
            double r_end   = std::hypot(portrait[static_cast<int>(i)].end_point().x() - cx, portrait[static_cast<int>(i)].end_point().y() - cy);
            INFO("trajectory " << i << ": r_start=" << r_start << " r_end=" << r_end);
            CHECK(r_end >= r_start);
        }
    }

    SECTION("export CSV") {
        std::string dir = "results/streamlines_single_injector";
        fs::create_directories(dir);
        export_streamlines_csv(dir + "/streamlines.csv", portrait);

        auto& snaps = field.GetVelocityField();
        export_velocity_csv(dir + "/velocity_field.csv", snaps.back(),
                            sc.nx(), sc.ny());

        std::ofstream meta(dir + "/metadata.json");
        meta << "{ \"case\": \"single_injector\", \"nx\": " << sc.nx()
             << ", \"ny\": " << sc.ny()
             << ", \"lx\": " << sc.lx()
             << ", \"ly\": " << sc.ly()
             << ", \"mode\": \"pathlines\", \"end_time\": " << last_time
             << ", \"n_rays\": " << n_rays
             << ", \"wells\": [{\"name\": \"INJ\", \"x\": " << cx << ", \"y\": " << cy << "}]"
             << " }";
    }
}

// ─── two wells ─────────────────────────────────────────────────────────────────

TEST_CASE("Streamlines: two wells",
          "[streamlines][two-well]")
{
    constexpr double sim_end = SL_sim_end;
    simulation_cases::TwoWellCase sc(SL_N, SL_N);
    auto sim = run_simulation(sc, sim_end);

    auto& field = sim->flowFields[0];
    REQUIRE_FALSE(field.is_empty());

    double last_time = sim_end;

    auto wells = sc.wells_info();
    double inj_x = wells[0].x, inj_y = wells[0].y;
    double prod_x = wells[1].x, prod_y = wells[1].y;

    double hx = sc.lx() / sc.nx();
    double r  = 1.0 * hx;
    int n_rays = SL_n_rays;

    auto starts = make_radial_starts(inj_x, inj_y, r, n_rays);
    PhasePortrait portrait;
    portrait.InstansiateTrajectories(starts, 0.0);
    portrait.FollowTrajectories(last_time, field, 1.0);

    SECTION("trajectories are not empty") {
        for (size_t i = 0; i < portrait.NumberOfTrajectories(); ++i) {
            INFO("trajectory " << i);
            REQUIRE(portrait[static_cast<int>(i)].size() >= 2);
        }
    }

    SECTION("all points are finite") {
        for (size_t i = 0; i < portrait.NumberOfTrajectories(); ++i)
            for (size_t j = 0; j < portrait[static_cast<int>(i)].size(); ++j) {
                INFO("traj=" << i << " pt=" << j);
                REQUIRE(std::isfinite(portrait[static_cast<int>(i)][j].x()));
                REQUIRE(std::isfinite(portrait[static_cast<int>(i)][j].y()));
            }
    }

    SECTION("trajectories advance toward producer") {
        double min_dist = std::numeric_limits<double>::max();
        for (size_t i = 0; i < portrait.NumberOfTrajectories(); ++i) {
            double d = std::hypot(portrait[static_cast<int>(i)].end_point().x() - prod_x,
                                  portrait[static_cast<int>(i)].end_point().y() - prod_y);
            min_dist = std::min(min_dist, d);
        }
        double start_dist = std::hypot(inj_x - prod_x, inj_y - prod_y);
        INFO("closest endpoint to PROD: " << min_dist
             << " m (INJ-PROD distance: " << start_dist << " m)");
        CHECK(min_dist < start_dist);
    }

    SECTION("export CSV") {
        std::string dir = "results/streamlines_two_well";
        fs::create_directories(dir);
        export_streamlines_csv(dir + "/streamlines.csv", portrait);

        auto& snaps = field.GetVelocityField();
        export_velocity_csv(dir + "/velocity_field.csv", snaps.back(),
                            sc.nx(), sc.ny());

        std::ofstream meta(dir + "/metadata.json");
        meta << "{ \"case\": \"two_well\", \"nx\": " << sc.nx()
             << ", \"ny\": " << sc.ny()
             << ", \"lx\": " << sc.lx()
             << ", \"ly\": " << sc.ly()
             << ", \"mode\": \"pathlines\", \"end_time\": " << last_time
             << ", \"n_rays\": " << n_rays
             << ", \"wells\": ["
             << "{\"name\": \"INJ\", \"x\": " << inj_x << ", \"y\": " << inj_y << "},"
             << "{\"name\": \"PROD\", \"x\": " << prod_x << ", \"y\": " << prod_y << "}"
             << "] }";
    }
}
