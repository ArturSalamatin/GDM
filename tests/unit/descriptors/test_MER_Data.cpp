#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Descriptors/MER_Descriptor.h"

using namespace reservoir_simulator::mer_descriptor;
using Catch::Approx;

static SingleMERrecord make_record(float time, float oil_v, float water_v,
                                   float oil_m, float water_m,
                                   float pump_water = 0.0f, float idle_time = 0.0f,
                                   float type = 1.0f, float is_work = 1.0f) {
    return {
        {L"time", time},
        {L"oil_v", oil_v},
        {L"water_v", water_v},
        {L"oil_m", oil_m},
        {L"water_m", water_m},
        {L"pump_water", pump_water},
        {L"idle_time", idle_time},
        {L"type", type},
        {L"is_work", is_work}
    };
}

TEST_CASE("MER_Data: default has zero records",
          "[unit][level2][descriptors][MER_Data]") {
    MER_Data md;
    CHECK(md.recordsSize() == 0);
}

TEST_CASE("MER_Data: construct with sorted records",
          "[unit][level2][descriptors][MER_Data]") {
    SingleWell_MER_Data data = {
        make_record(30.0f, 10.0f, 5.0f, 8000.0f, 5000.0f),
        make_record(60.0f, 12.0f, 6.0f, 9600.0f, 6000.0f),
        make_record(90.0f, 8.0f, 3.0f, 6400.0f, 3000.0f)
    };
    std::wstring name = L"TestWell";
    MER_Data md(name, data);

    CHECK(md.recordsSize() == 3);
    CHECK(md.firstRecordDate() == Approx(30.0));
    CHECK(md.lastRecordDate() == Approx(90.0));
}

TEST_CASE("MER_Data: unsorted records get sorted by time",
          "[unit][level2][descriptors][MER_Data]") {
    SingleWell_MER_Data data = {
        make_record(90.0f, 8.0f, 3.0f, 6400.0f, 3000.0f),
        make_record(30.0f, 10.0f, 5.0f, 8000.0f, 5000.0f),
        make_record(60.0f, 12.0f, 6.0f, 9600.0f, 6000.0f)
    };
    std::wstring name = L"TestWell";
    MER_Data md(name, data);

    CHECK(md.firstRecordDate() == Approx(30.0));
    CHECK(md.lastRecordDate() == Approx(90.0));
}

TEST_CASE("MER_Data: debitPartial returns mass rate",
          "[unit][level2][descriptors][MER_Data]") {
    SingleWell_MER_Data data = {
        make_record(30.0f, 10.0f, 5.0f, 300.0f, 150.0f),
        make_record(60.0f, 12.0f, 6.0f, 360.0f, 180.0f)
    };
    std::wstring name = L"W1";
    MER_Data md(name, data);

    auto& debit = md.debitPartial(45.0);
    CHECK(debit.oil == Approx(360.0 / 30.0));
    CHECK(debit.water == Approx(180.0 / 30.0));
}

TEST_CASE("MER_Data: CleanMER_record removes leading zero-debit",
          "[unit][level2][descriptors][MER_Data]") {
    SingleWell_MER_Data data = {
        make_record(30.0f, 0.0f, 0.0f, 0.0f, 0.0f),
        make_record(60.0f, 0.0f, 0.0f, 0.0f, 0.0f),
        make_record(90.0f, 10.0f, 5.0f, 8000.0f, 5000.0f),
        make_record(120.0f, 12.0f, 6.0f, 9600.0f, 6000.0f)
    };
    std::wstring name = L"W2";
    MER_Data md(name, data);
    md.CleanMER_record();

    CHECK(md.recordsSize() == 2);
    CHECK(md.firstRecordDate() == Approx(90.0));
}

TEST_CASE("MER_Data: knownExploitationPeriod covers data range",
          "[unit][level2][descriptors][MER_Data]") {
    SingleWell_MER_Data data = {
        make_record(100.0f, 10.0f, 5.0f, 8000.0f, 5000.0f),
        make_record(200.0f, 12.0f, 6.0f, 9600.0f, 6000.0f)
    };
    std::wstring name = L"W3";
    MER_Data md(name, data);

    auto period = md.knownExploitationPeriod();
    CHECK(period.start < 100.0);
    CHECK(period.end == Approx(200.0));
}
