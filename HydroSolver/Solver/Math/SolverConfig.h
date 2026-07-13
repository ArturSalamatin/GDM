#pragma once

#include <amgcl/adapter/crs_tuple.hpp>
#include <amgcl/make_solver.hpp>
#include <amgcl/amg.hpp>
#include <amgcl/coarsening/aggregation.hpp>
#include <amgcl/relaxation/ilu0.hpp>
#include <amgcl/relaxation/as_preconditioner.hpp>
#include <amgcl/solver/lgmres.hpp>

#if defined(GDM_SOLVER_CPR_DRS)
#include <amgcl/preconditioner/cpr_drs.hpp>
#elif defined(GDM_SOLVER_CPR_SA)
#include <amgcl/preconditioner/cpr.hpp>
#include <amgcl/coarsening/smoothed_aggregation.hpp>
#elif defined(GDM_SOLVER_CPR_BICGSTAB)
#include <amgcl/preconditioner/cpr.hpp>
#include <amgcl/solver/bicgstab.hpp>
#elif defined(GDM_SOLVER_ILU0)
// no CPR header needed
#else
// Default: GDM_SOLVER_CPR or undefined
#include <amgcl/preconditioner/cpr.hpp>
#endif

namespace reservoir_simulator {
namespace linear_problem {

using ScalarBackend = amgcl::backend::builtin<double>;

#if defined(GDM_SOLVER_CPR_DRS)

using PrecondType = amgcl::preconditioner::cpr_drs<
    amgcl::amg<ScalarBackend, amgcl::coarsening::aggregation, amgcl::relaxation::ilu0>,
    amgcl::relaxation::as_preconditioner<ScalarBackend, amgcl::relaxation::ilu0>
>;
using SolverType = amgcl::make_solver<PrecondType, amgcl::solver::lgmres<ScalarBackend>>;

#elif defined(GDM_SOLVER_CPR_SA)

using PrecondType = amgcl::preconditioner::cpr<
    amgcl::amg<ScalarBackend, amgcl::coarsening::smoothed_aggregation, amgcl::relaxation::ilu0>,
    amgcl::relaxation::as_preconditioner<ScalarBackend, amgcl::relaxation::ilu0>,
    amgcl::preconditioner::true_impes_weights
>;
using SolverType = amgcl::make_solver<PrecondType, amgcl::solver::lgmres<ScalarBackend>>;

#elif defined(GDM_SOLVER_CPR_BICGSTAB)

using PrecondType = amgcl::preconditioner::cpr<
    amgcl::amg<ScalarBackend, amgcl::coarsening::aggregation, amgcl::relaxation::ilu0>,
    amgcl::relaxation::as_preconditioner<ScalarBackend, amgcl::relaxation::ilu0>,
    amgcl::preconditioner::true_impes_weights
>;
using SolverType = amgcl::make_solver<PrecondType, amgcl::solver::bicgstab<ScalarBackend>>;

#elif defined(GDM_SOLVER_ILU0)

using PrecondType = amgcl::relaxation::as_preconditioner<ScalarBackend, amgcl::relaxation::ilu0>;
using SolverType = amgcl::make_solver<PrecondType, amgcl::solver::lgmres<ScalarBackend>>;

#else
// Default: CPR<AMG<aggregation, ilu0>, ilu0> + lgmres

using PrecondType = amgcl::preconditioner::cpr<
    amgcl::amg<ScalarBackend, amgcl::coarsening::aggregation, amgcl::relaxation::ilu0>,
    amgcl::relaxation::as_preconditioner<ScalarBackend, amgcl::relaxation::ilu0>,
    amgcl::preconditioner::true_impes_weights
>;
using SolverType = amgcl::make_solver<PrecondType, amgcl::solver::lgmres<ScalarBackend>>;

#endif

} // namespace linear_problem
} // namespace reservoir_simulator
