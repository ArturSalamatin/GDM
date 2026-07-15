#pragma once
#include "../stdafx.h"

#include "FlowField/Point.h"
#include "FlowField/SomeFlowField.h"
#include "../Solver/Math/MathRoutines.h"

namespace reservoir_simulator
{
	namespace phasePortrait
	{

		Direction f(const trPoint& p, SomeFlowField& field);
		Direction f_const(const trPoint& p, SomeFlowField& field, double time);

		class Trajectory
		{
		private:
			std::vector<trPoint> points;

		public:
			void FollowTrajectory(double endTime, SomeFlowField& field, double dt)
			{
				points.reserve(100);
				math_routines::MathRoutines::IntegrateODE<SomeFlowField>(endTime, points, dt, field, &f);
				points.shrink_to_fit();
			}
			void FollowTrajectory_fixed_time(double endTime, SomeFlowField& field, double dt, double fixed_time)
			{
			//	std::cout << fixed_time << ' ' << std::flush;
				auto glambda = [fixed_time](const trPoint& p, SomeFlowField& field) -> Direction {
					return f_const(p, field, fixed_time);
				};

				points.reserve(100);
				math_routines::MathRoutines::IntegrateODE<SomeFlowField>(endTime, points, dt, field, glambda);
				points.shrink_to_fit();
			}

		public:
		//	Trajectory() {}
			Trajectory(const trPoint& p, const int capacity = 2) { points.reserve(capacity); points.push_back(p); }
			void push_back(const trPoint& p) { points.push_back(p); }
			const trPoint& back() const { return points.back(); }
			void pop_back() { points.pop_back(); }
			void shrink_to_fit() { points.shrink_to_fit(); }
			size_t size() const { return points.size(); }
			const trPoint& operator[](std::size_t idx) const { return points[idx]; }

			const trPoint& start_point() const
			{
				return points.front();
			}

			const trPoint& end_point() const
			{
				return points.back();
			}
			std::string PrintTrajectory() const
			{
				char buffer[200];
				snprintf(buffer, 200, "%u;", (unsigned int)points.size());
				std::string result{ buffer };
				for (int l = 0; l < points.size(); l++)
				{
					char buffer[700];
					snprintf(buffer, 700, "%+19.11E;%+19.11E;", points[l].x(), points[l].y());
					result += buffer;
				}
				return result;
			}
		};
	} // well
} // reservoir_simulator



