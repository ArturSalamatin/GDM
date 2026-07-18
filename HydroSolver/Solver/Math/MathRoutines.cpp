#include "MathRoutines.h"

 double math_routines::MathRoutines::LinearInterp(double queryT, double t1, double t2, double v1, double v2)
{
	if (t1 == t2)
		return (v1 + v2) / 2;
	return (v2 * (queryT - t1) + v1 * (t2 - queryT)) / (t2 - t1);
}

 std::pair<double, double> math_routines::MathRoutines::LinearInterp(double queryT, double t1, double t2, const std::pair<double, double>& v1, const std::pair<double, double>& v2)
{
	return std::make_pair(LinearInterp(queryT, t1, t2, v1.first, v2.first), LinearInterp(queryT, t1, t2, v1.second, v2.second));
}

/// <summary>
/// x_mesh reflects internal std::vector in the field variable
/// </summary>
/// <param name="queryP"></param>
/// <param name="x_mesh"></param>
/// <param name="y_mesh"></param>
/// <param name="field"></param>
/// <returns></returns>

 double math_routines::MathRoutines::InterpFieldConstTime(const reservoir_simulator::phasePortrait::Point& queryP, const std::vector<double>& x_mesh, const std::vector<double>& y_mesh, const std::vector<std::vector<double>>& field)
{
	ptrdiff_t x_idx = MathRoutines::LowerPointUniformMesh(x_mesh, queryP.x());
	if (x_idx < 0 || x_idx > static_cast<ptrdiff_t>(x_mesh.size()) - 2)
		return NAN;
	ptrdiff_t y_idx = MathRoutines::LowerPointUniformMesh(y_mesh, queryP.y());
	if (y_idx < 0 || y_idx > static_cast<ptrdiff_t>(y_mesh.size()) - 2)
		return NAN;


	// instansiate 4 points surrunding the query point
	const std::array<reservoir_simulator::phasePortrait::Point, 4> P{
		reservoir_simulator::phasePortrait::Point(x_mesh[x_idx], y_mesh[y_idx]), reservoir_simulator::phasePortrait::Point(x_mesh[x_idx], y_mesh[y_idx + 1]),
		reservoir_simulator::phasePortrait::Point(x_mesh[x_idx + 1], y_mesh[y_idx + 1]), reservoir_simulator::phasePortrait::Point(x_mesh[x_idx + 1], y_mesh[y_idx]) };
	// instantiate respective field values
	const std::array<double, 4>& vals{
		field[y_idx][x_idx], field[y_idx + 1][x_idx], field[y_idx + 1][x_idx + 1], field[y_idx][x_idx + 1] };

	return MathRoutines::BilinearInterp(queryP, P, vals);
}

 double math_routines::MathRoutines::BilinearInterp(const reservoir_simulator::phasePortrait::Point& queryP, const std::array<reservoir_simulator::phasePortrait::Point, 4>& P, const std::array<double, 4>& vals)
{
	auto A = reservoir_simulator::phasePortrait::Point::Area(queryP, P);
	auto A0 = A[0] + A[1] + A[2] + A[3];
	return (A[0] * vals[2] + A[1] * vals[3] + A[2] * vals[0] + A[3] * vals[1]) / A0;
}

 ptrdiff_t math_routines::MathRoutines::LowerPointUniformMesh(const std::vector<double>& mesh, double queryX)
{
	double hx = mesh[1] - mesh[0]; // get mesh step
	double x0 = mesh[0]; // get origin
	return static_cast<ptrdiff_t>(std::floor((queryX - x0) / hx));
}

 ptrdiff_t math_routines::MathRoutines::LowerPointNonUniformMesh(const std::vector<double>& mesh, double queryX)
{
	if (queryX < mesh[0] || queryX > mesh.back())
		return -1;
	auto iter = std::distance(mesh.begin(), upper_bound(mesh.begin(), mesh.end(), queryX)) - 1;
	return iter;
}

 std::array<double, 3> math_routines::MathRoutines::prod(std::array<double, 3> arr, double s)
{
	return { arr[0] * s, arr[1] * s, arr[2] * s };
}

 std::array<double, 3> math_routines::MathRoutines::sum(const std::array<double, 3> arr1, const std::array<double, 3> arr2)
{
	return{ arr1[0] + arr2[0], arr1[1] + arr2[1], arr1[2] + arr2[2] };
}

 reservoir_simulator::phasePortrait::trPoint math_routines::MathRoutines::sum(const reservoir_simulator::phasePortrait::trPoint& P, const std::array<double, 3> arr2, double ds)
{

	return  reservoir_simulator::phasePortrait::trPoint(P.t() + arr2[0], P.s() + ds, P.x() + arr2[1], P.y() + arr2[2]);
}
