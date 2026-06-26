#include "example_runner.h"
#include "../tests/simulation_cases/TwoWellCase.h"

int main() {
    simulation_cases::TwoWellCase sc(31, 31);
    examples::run_case(sc, "results");
    return 0;
}
