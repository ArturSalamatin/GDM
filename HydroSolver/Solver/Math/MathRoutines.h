#pragma once
#include "../../stdafx.h"

#include "../../Anomaly/FlowField/Point.h"

#undef min
#undef max

using namespace reservoir_simulator::phasePortrait;

namespace math_routines
{
	class MathRoutines
	{
		// interpolation routines
	public:

		/*static double InterpFieldConstTime(const reservoir_simulator::phasePortrait::Point& queryP, 
			double queryT, const std::vector<double>& x_mesh, const std::vector<double>& y_mesh,
			const std::vector<std::vector<double>>& field1, const std::vector<std::vector<double>>& field2,
			double t1, double t2)
		{
			double v1 = InterpFieldConstTime(queryP, x_mesh, y_mesh, field1);
			double v2 = InterpFieldConstTime(queryP, x_mesh, y_mesh, field2);

			return LinearInterp(queryT, t1, t2, v1, v2);

		}*/

		static double LinearInterp(double queryT, double t1, double t2, double v1, double v2);

		static std::pair<double, double> LinearInterp(double queryT, double t1, double t2,
			const std::pair<double, double>& v1, const std::pair<double, double>& v2);

		/// <summary>
		/// x_mesh reflects internal std::vector in the field variable
		/// </summary>
		/// <param name="queryP"></param>
		/// <param name="x_mesh"></param>
		/// <param name="y_mesh"></param>
		/// <param name="field"></param>
		/// <returns></returns>
		static double InterpFieldConstTime(const reservoir_simulator::phasePortrait::Point& queryP, 
			const std::vector<double>& x_mesh, const std::vector<double>& y_mesh,
			const std::vector<std::vector<double>>& field);

		static double BilinearInterp(const reservoir_simulator::phasePortrait::Point& queryP, 
			const std::array<reservoir_simulator::phasePortrait::Point, 4>& P, const std::array<double, 4>& vals);

		static int LowerPointUniformMesh(const std::vector<double>& mesh, double queryX);

		static int LowerPointNonUniformMesh(const std::vector<double>& mesh, double queryX);

		// integration routines
	public:

		template<typename Field>
		static void IntegrateODE(double endTime,
			std::vector<reservoir_simulator::phasePortrait::trPoint>& result,
			double dt_accurate, Field& field, Direction(*myFunc)(const reservoir_simulator::phasePortrait::trPoint&, Field&));

		template<typename Field>
		static void IntegrateODE_fixed_timestep(double endTime,
			std::vector<reservoir_simulator::phasePortrait::trPoint>& result,
			double dt_global, Field& field, Direction(*myFunc)(const reservoir_simulator::phasePortrait::trPoint&, Field&));

		template<typename Field, typename Functor>
		static void IntegrateODE(double endTime, 
			std::vector<reservoir_simulator::phasePortrait::trPoint>& result,
			double dt_accurate, Field& field, Functor& myFunc);

		template<typename Field>
		static reservoir_simulator::phasePortrait::trPoint Euler2(const  reservoir_simulator::phasePortrait::trPoint& P,
			double dt, Field& field, Direction(*myFunc)(const reservoir_simulator::phasePortrait::trPoint&, Field&));

		template<typename Field, typename Functor>
		static reservoir_simulator::phasePortrait::trPoint Euler2(const  reservoir_simulator::phasePortrait::trPoint& P,
			double dt, Field& field, Functor& myFunc);

		template<typename Field>
		static trPoint RK4(const reservoir_simulator::phasePortrait::trPoint& P,
			double ds, Field& field, Direction(*myFunc)(const reservoir_simulator::phasePortrait::trPoint&, Field&));


	private:
		static std::array<double, 3> prod(std::array<double, 3> arr, double s);
		static std::array<double, 3> sum(const std::array<double, 3> arr1, const std::array<double, 3> arr2);
		static reservoir_simulator::phasePortrait::trPoint sum(const reservoir_simulator::phasePortrait::trPoint& P, const std::array<double, 3> arr2, double ds);
	};

	template<typename Field>
	inline void MathRoutines::IntegrateODE(double endTime, std::vector<reservoir_simulator::phasePortrait::trPoint>& result, double dt_accurate, Field& field, Direction(*myFunc)(const reservoir_simulator::phasePortrait::trPoint&, Field&))
	{
		double curTime, startTime, dt = dt_accurate, ds;
		try {
			curTime = result.back().t();
			startTime = curTime;
			if (curTime == endTime)
				return; // current point is at the trajectory end
		}
		catch (std::exception&)
		{
			throw std::exception("Trajectory was not instantiated properly in MathRoutines class, IntegrateODE method.");
		}

		trPoint P_temp{ result.back() }, P{ P_temp };
		while ((endTime - curTime) * dt > 0) // we keep going in the right direction, towards endTime
		{
			if (dt > 0)
				dt = std::min(endTime - curTime, std::min(dt * 2, dt_accurate));
			else
				dt = std::max(endTime - curTime, std::max(dt * 2, dt_accurate));

			P = Euler2(P_temp, dt, field, myFunc);
			ds = P.P().distance(P_temp.P());
			while (ds > 1)
			{
				dt /= (ds + 1);
				P = Euler2(P_temp, dt, field, myFunc);
				ds = P.P().distance(P_temp.P());
			}
			curTime = P.t();

			if (P.is_normal())
			{
				//if (result.size() > 1)
				//{ // check for rapid direction change
				//	int size = result.size();
				//	std::pair<double, double> dl1, dl2;
				//	dl1 = std::make_pair(P_temp.x() - result[size-2].x(), P_temp.y() - result[size-2].y());
				//	dl2 = std::make_pair(P.x() - P_temp.x(), P.y() - P_temp.y());
				//if (dl1.first * dl2.first + dl1.second * dl2.second < 0.0)
				//{// direction changed rapidly. Sink is encountered
				//	trPoint Pp = result.back();
				//	result.pop_back();
				//	result.push_back((P + Pp) / 2.0);
				//	return;
				//}
				//}
				if (P.P().distance(result.back().P()) > 2.0)
				{
					result.push_back(P);
					P_temp = result.back();
				}
				else
					P_temp = P;
			}
			else
			{// current point is bad for some reason...
				return;
			}
		}

		if ((result.size() == 1) ||
			(P.P().distance(result.back().P()) > 0.0))
			result.push_back(P);
	}

	template<typename Field>
	inline void MathRoutines::IntegrateODE_fixed_timestep(double endTime, std::vector<reservoir_simulator::phasePortrait::trPoint>& result, double dt_global, Field& field, Direction(*myFunc)(const reservoir_simulator::phasePortrait::trPoint&, Field&))
	{
		// only save points that are exactly dt_init different
		const double dt_accurate = 2;
		double curTime, startTime, dt = dt_accurate, ds;
		try {
			curTime = result.back().t();
			startTime = curTime;
			if (curTime == endTime)
				return; // current point is at the trajectory end
		}
		catch (std::exception& e)
		{
			throw std::exception("Trajectory was not instantiated properly in MathRoutines class, IntegrateODE method.");
		}

		double next_ref_time = startTime + dt_global;
		double dt_temp = 0.0;
		trPoint P_temp{ result.back() }, P{ P_temp };
		while ((endTime - curTime) * dt > 0) // we keep going in the right direction, towards endTime
		{
			dt_temp = dt;
			if (dt > 0)
				dt = std::min(std::min(endTime - curTime, std::min(dt * 2, dt_accurate)), next_ref_time - curTime);
			else
				dt = std::max(std::max(endTime - curTime, std::max(dt * 2, dt_accurate)), next_ref_time - curTime);

			P = Euler2(P_temp, dt, field, myFunc);
			ds = P.P().distance(P_temp.P());
			while (ds > 1)
			{
				dt /= (ds + 1);
				P = Euler2(P_temp, dt, field, myFunc);
				ds = P.P().distance(P_temp.P());
			}
			curTime = P.t();

			if (P.is_normal())
			{
				//if (result.size() > 1)
				//{ // check for rapid direction change
				//	int size = result.size();
				//	std::pair<double, double> dl1, dl2;
				//	dl1 = std::make_pair(P_temp.x() - result[size-2].x(), P_temp.y() - result[size-2].y());
				//	dl2 = std::make_pair(P.x() - P_temp.x(), P.y() - P_temp.y());
				//if (dl1.first * dl2.first + dl1.second * dl2.second < 0.0)
				//{// direction changed rapidly. Sink is encountered
				//	trPoint Pp = result.back();
				//	result.pop_back();
				//	result.push_back((P + Pp) / 2.0);
				//	return;
				//}
				//}
				/*
				if (P.P().distance(result.back().P()) > 2.0)
				{
				result.push_back(P);
				P_temp = result.back();
				}*/
				if (abs(abs(curTime - result.back().t()) - dt_global) < 1E-10)
				{
					dt = dt_temp;
					result.push_back(P);
					P_temp = result.back();
					next_ref_time += dt_global;
				}
				else
					P_temp = P;
			}
			else
			{// current point is bad for some reason...
				return;
			}
		}

		if ((result.size() == 1) || (abs(abs(curTime - result.back().t()) - dt_global) < 1E-10)
			//(P.P().distance(result.back().P()) > 0.0)
			)
			result.push_back(P);
		if (result.size() < 151)
			std::wcout << result.size() << std::endl;
	}

	template<typename Field, typename Functor>
	inline void MathRoutines::IntegrateODE(double endTime, std::vector<reservoir_simulator::phasePortrait::trPoint>& result, double dt_accurate, Field& field, Functor& myFunc)
	{
		double curTime, startTime, dt = dt_accurate, ds;
		try {
			curTime = result.back().t();
			startTime = curTime;
			if (curTime == endTime)
				return; // current point is at the trajectory end
		}
		catch (std::exception&)
		{
			throw std::exception("Trajectory was not instantiated properly in MathRoutines class, IntegrateODE method.");
		}

		trPoint P_temp{ result.back() }, P{ P_temp };
		while ((endTime - curTime) * dt > 0) // we keep going in the right direction, towards endTime
		{
			if (dt > 0)
				dt = std::min(endTime - curTime, std::min(dt * 2, dt_accurate));
			else
				dt = std::max(endTime - curTime, std::max(dt * 2, dt_accurate));

			P = Euler2(P_temp, dt, field, myFunc);
			ds = P.P().distance(P_temp.P());
			while (ds > 1)
			{
				dt /= (ds + 1);
				P = Euler2(P_temp, dt, field, myFunc);
				ds = P.P().distance(P_temp.P());
			}
			curTime = P.t();

			if (P.is_normal())
			{
				//if (result.size() > 1)
				//{ // check for rapid direction change
				//	int size = result.size();
				//	std::pair<double, double> dl1, dl2;
				//	dl1 = std::make_pair(P_temp.x() - result[size-2].x(), P_temp.y() - result[size-2].y());
				//	dl2 = std::make_pair(P.x() - P_temp.x(), P.y() - P_temp.y());
				//if (dl1.first * dl2.first + dl1.second * dl2.second < 0.0)
				//{// direction changed rapidly. Sink is encountered
				//	trPoint Pp = result.back();
				//	result.pop_back();
				//	result.push_back((P + Pp) / 2.0);
				//	return;
				//}
				//}
				if (P.P().distance(result.back().P()) > 2.0)
				{
					result.push_back(P);
					P_temp = result.back();
				}
				else
					P_temp = P;
			}
			else
			{// current point is bad for some reason...
				return;
			}
		}

		if ((result.size() == 1) ||
			(P.P().distance(result.back().P()) > 0.0))
			result.push_back(P);
		/*double dist = P.P().distance(result.back().P());
		std::cout << dist << ' ' << std::flush;
		if ((result.size() == 1) ||
		(dist < 1))
		result.push_back(P);*/

	}

	template<typename Field>
	inline reservoir_simulator::phasePortrait::trPoint MathRoutines::Euler2(const reservoir_simulator::phasePortrait::trPoint& P, double dt, Field& field, Direction(*myFunc)(const reservoir_simulator::phasePortrait::trPoint&, Field&))
	{
		reservoir_simulator::phasePortrait::trPoint P1_2 = P.Move(myFunc(P, field), dt / 2.0);
		if (P1_2.is_normal())
		{
			reservoir_simulator::phasePortrait::trPoint P1 = P.Move(myFunc(P1_2, field), dt);
			return P1;
		}
		else
		{
			return P;
		}
	}

	template<typename Field, typename Functor>
	inline reservoir_simulator::phasePortrait::trPoint MathRoutines::Euler2(const reservoir_simulator::phasePortrait::trPoint& P, double dt, Field& field, Functor& myFunc)
	{
		reservoir_simulator::phasePortrait::trPoint P1_2 = P.Move(myFunc(P, field), dt / 2.0);
		if (P1_2.is_normal())
		{
			reservoir_simulator::phasePortrait::trPoint P1 = P.Move(myFunc(P1_2, field), dt);
			return P1;
		}
		else
		{
			return P;
		}
	}

	template<typename Field>
	inline trPoint MathRoutines::RK4(const reservoir_simulator::phasePortrait::trPoint& P, double ds, Field& field, Direction(*myFunc)(const reservoir_simulator::phasePortrait::trPoint&, Field&))
	{

		const auto& P1 = P;
		auto k1 = myFunc(P, field);
		auto P2 = P.Move(k1, ds / 2.0);
		auto k2 = myFunc(P2, field);
		auto P3 = P.Move(k2, ds / 2.0);
		auto k3 = myFunc(P3, field);
		auto P4 = P.Move(k3, ds);
		auto k4 = myFunc(P4, field);


		return sum(P,
			prod(
				sum(k1,
					sum(prod(k2, 2.0),
						sum(prod(k3, 2.0), k4))), ds / 6), ds);
	}


} //mathRoutines