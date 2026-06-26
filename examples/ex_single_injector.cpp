#include "example_runner.h"
#include "../tests/simulation_cases/SingleInjectorCase.h"

int main() {
    simulation_cases::SingleInjectorCase sc(21, 21);
    examples::run_case(sc, "results");
    return 0;
}
