#pragma once
#include "../../stdafx.h"

//#define USE_PARALLEL

#include "SomeFlowField.h"
#include "Point.h"
#include "../Trajectory.h"

#include "../../Helpers/Defines.h"
#include "../../Utils/Defines.h"

namespace reservoir_simulator
{
	namespace phasePortrait
	{
		/// <summary>
		/// Phase portrait for the problem
		/// We consider (x,y,t) as searched-for functions of distance s along the trajectory
		/// </summary>
		/// <typeparam name="trPoint_"></typeparam>
		class PhasePortrait
		{
		private:
			std::vector<Trajectory> trajectoryEnsemble;

		public:

			static std::vector<phasePortrait::Point> DistributeStartPoints_radial(const std::map<std::wstring, double> data)
			{
				double x0 = data.at(L"x0"), y0 = data.at(L"y0"), r = data.at(L"r"), count = data.at(L"count");

				constexpr auto PI = 3.141592653589793;
				std::vector<reservoir_simulator::phasePortrait::Point> startPoints;
				double dPhi = 2 * PI / count;

				for (int i = 0; i < count; i++)
					startPoints.push_back(reservoir_simulator::phasePortrait::Point(x0 + r * cos(i * dPhi), y0 + r * sin(i * dPhi)));
				return startPoints;
			}

			static std::vector<phasePortrait::Point> DistributeStartPoints_rectangle(std::vector<double> xmesh, std::vector<double> ymesh)
			{
				double hx{ xmesh[1] - xmesh[0] };
				double hy{ ymesh[1] - ymesh[0] };

				double xmin = xmesh[0] - hx / 2.0,
					xmax = xmesh.back() + hx / 2.0;
				double ymin = ymesh[0] - hy / 2.0,
					ymax = ymesh.back() + hy / 2.0;

				int n = 50;
				hx = (xmax - xmin) / n;
				hy = (ymax - ymin) / n;

				std::vector<reservoir_simulator::phasePortrait::Point> startPoints;
				for (int j = 0; j < n; j++)
					for (int i = 1; i < n; i++)
						startPoints.push_back(reservoir_simulator::phasePortrait::Point(hx / 2 + i * hx + xmin, ymin + hy / 2 + j * hy));
				return startPoints;
			}

			static std::vector<phasePortrait::Point> DistributeStartPoints_rectangle(const std::map<std::wstring, double> data)
			{
				double xmin = data.at(L"xmin");
				double ymin = data.at(L"ymin");
				double xmax = data.at(L"xmax");
				double ymax = data.at(L"ymax");

				int n = 15;
				double hx = (xmax - xmin) / (n - 1);
				double hy = (ymax - ymin) / (n - 1);

				std::vector<reservoir_simulator::phasePortrait::Point> startPoints;
				for (int j = 0; j < n; j++)
					for (int i = 1; i < n; i++)
						startPoints.push_back(reservoir_simulator::phasePortrait::Point(hx / 2 + i * hx + xmin, ymin + hy / 2 + j * hy));
				return startPoints;
			}

			/*void InstantiateTrajectories(double x0, double y0, double r, double count, double startTime)
			{
				InstansiateTrajectories(DistributeStartPoints_radial(x0, y0, r, count), startTime);
			}*/

			void InstansiateTrajectories(std::vector<reservoir_simulator::phasePortrait::Point>& startPoints, double startTime)
			{
				for (const auto& P : startPoints)
					trajectoryEnsemble.emplace_back(reservoir_simulator::phasePortrait::trPoint(startTime, 0.0, P));
			}

			void FollowTrajectories(double endTime, SomeFlowField& field, double dt)
			{
				for (int i = 0; i < NumberOfTrajectories(); i++)
					trajectoryEnsemble[i].FollowTrajectory(endTime, field, dt);
			}

			void FollowTrajectories_fixed_time(double endTime, SomeFlowField& field, double dt, double fixed_time)
			{
				for (int i = 0; i < NumberOfTrajectories(); i++)
					trajectoryEnsemble[i].FollowTrajectory_fixed_time(endTime, field, dt, fixed_time);
			}

			void improve_discretization(double endTime, SomeFlowField& field, double dt, double fixed_time, std::map<std::wstring, double> data)
			{
				constexpr auto PI = 3.141592653589793;
				std::vector<double> phi{ 0,PI,2 * PI };
				phi.reserve(100);

				double r = data[L"r"];
				double x0 = data[L"x0"];
				double y0 = data[L"y0"];
				double dist_abs = data[L"dist_abs"];
				double dist_rel = data[L"dist_rel"];
				double t0 = data[L"start_time"];

				int tr_id = 0; // start with the first trajectory
				double dist_forward = 0; // must be less than 1 meter
				double p = 0; // overall perimeter
				size_t tr_count = trajectoryEnsemble.size(); // current overall number of trajectories
				double d_phi;
				bool f;
				while (tr_id < tr_count)
				{
					do
					{
						d_phi = phi[tr_id + 1] - phi[tr_id];
						dist_forward = trajectoryEnsemble[tr_id].end_point().P().distance(trajectoryEnsemble[(tr_id + 1) % tr_count].end_point().P());
						p = perimeter();
						f = d_phi > PI/1000 && 
							(dist_forward > dist_abs || dist_forward * dist_rel > p);
						if (f) {
							double phi_ = (phi[tr_id] + phi[tr_id + 1]) / 2;
							phi.insert(phi.begin() + tr_id + 1, phi_);
							trPoint pm{ t0, 0.0, x0 + r * cos(phi_),y0 + r * sin(phi_) };
							trajectoryEnsemble.insert(trajectoryEnsemble.begin() + tr_id + 1, Trajectory(pm, 100));
							tr_count = trajectoryEnsemble.size();
							trajectoryEnsemble[tr_id + 1].FollowTrajectory_fixed_time(endTime, field, dt, fixed_time);
						}
					} while (f);
					tr_id++;
				}
			}

			/*reservoir_simulator::phasePortrait::Contour Contour() const
			{
				auto result = make_pair(std::vector<double>(), std::vector<double>());
				for (int i = 0; i < trajectoryEnsemble.size(); i++)
				{
					result.first.push_back(trajectoryEnsemble[i].back().x());
					result.second.push_back(trajectoryEnsemble[i].back().y());
				}
				result.first.push_back(trajectoryEnsemble[0].back().x());
				result.second.push_back(trajectoryEnsemble[0].back().y());

				return reservoir_simulator::phasePortrait::Contour(result.first, result.second);
			}

			std::vector<Coordinate> Contour_geos() const
			{
				std::vector<Coordinate> result{};
				for (int i = 0; i < trajectoryEnsemble.size(); i++)
					result.emplace_back(trajectoryEnsemble[i].back().x(), trajectoryEnsemble[i].back().y());
				result.emplace_back(trajectoryEnsemble[0].back().x(), trajectoryEnsemble[0].back().y());
				return result;
			}

			std::pair<std::vector<double>, std::vector<double>>  ContourReverse() const
			{
				auto result = make_pair(std::vector<double>(), std::vector<double>());
				for (int i = trajectoryEnsemble.size() - 1; i > -1; i--)
				{
					result.first.push_back(trajectoryEnsemble[i].back().x());
					result.second.push_back(trajectoryEnsemble[i].back().y());
				}
				result.first.push_back(trajectoryEnsemble.back().back().x());
				result.second.push_back(trajectoryEnsemble.back().back().y());

				return result;
			}*/


			/*std::vector<reservoir_simulator::phasePortrait::trPoint> StartPoints() const
			{
				std::vector<reservoir_simulator::phasePortrait::trPoint> result;
				for (int i = 0; i < trajectoryEnsemble.size(); i++)
					result.push_back(trajectoryEnsemble[i].start_point());
				return result;
			}*/

			size_t NumberOfTrajectories() const { return trajectoryEnsemble.size(); }

			const reservoir_simulator::phasePortrait::Trajectory& operator[] (int i) const {
				return trajectoryEnsemble[i];
			}

			/*std::wstring PrintPhasePortrait() const
			{
				int trCount = 0;
				std::wstring result;
				for (int i = 0; i < NumberOfTrajectories(); i++)
				{
					if (trajectoryEnsemble[i].size() > 1)
					{
						trCount++;
						result += trajectoryEnsemble[i].PrintTrajectory();
					}
				}

				wchar_t buffer[30];
				swprintf(buffer, 30, L"%u;", trCount);
				std::wstring result0{ buffer };

				return result0 + result;
			}*/

			double perimeter()
			{
				double p = 0;
				for (int i = 0; i < trajectoryEnsemble.size(); i++)
					p += trajectoryEnsemble[i].end_point().P().distance(trajectoryEnsemble[(i + 1) % trajectoryEnsemble.size()].end_point().P());

				return p;
			}

			geos_polygon create_geos_polygon()
			{
				std::vector<Coordinate> result;
				for (int i = 0; i < trajectoryEnsemble.size(); i++)
					result.emplace_back(trajectoryEnsemble[i].back().x(), trajectoryEnsemble[i].back().y());
				result.emplace_back(trajectoryEnsemble[0].back().x(), trajectoryEnsemble[0].back().y());
				if (result.empty())
					std::cout << "bliayat!";
				return geos_polygon{ std::move(result) };

			}
		};

		class FlowField
		{
		public:
			using FlowFieldSequence = std::vector<FlowField>;
			using Features = struct { std::wstring layer_name; };
		protected:
			SomeFlowField::Ptr field_ptr;
			Features features;

		//	std::vector<PhasePortrait> phasePortraitEnsemble;

		//	static std::mutex mtx;

		public:

			const std::wstring& layer_name() { return features.layer_name; }

			FlowField(const SomeFlowField::Ptr& f, const std::wstring& layer_name) :field_ptr{ f }, features{ layer_name } {}

			//void FollowPhasePortrait(const std::vector<phasePortrait::Point>& pVector, double startTime, double endTime, double dt)
			//{
			//	dt = abs(dt); // assume we go forward in time
			//	if (endTime < startTime) dt *= (-1); // the trajectory is followed back in time

			//	phasePortraitEnsemble.push_back(reservoir_simulator::phasePortrait::PhasePortrait());
			//	phasePortraitEnsemble.back().InstansiateTrajectories(pVector, startTime);
			//	phasePortraitEnsemble.back().FollowTrajectories(endTime, *field_ptr.get(), //field, 
			//		dt);
			//}

			geos_polygon FollowPhasePortrait_fixed_time(std::vector<phasePortrait::Point>&& pVector,
				double startTime, double endTime, double dt, double fixed_time, std::map<std::wstring, double> data)
			{
				dt = abs(dt); // assume we go forward in time
				if (endTime < startTime) dt *= (-1); // the trajectory is followed back in time

			//	phasePortraitEnsemble.emplace_back();//reservoir_simulator::phasePortrait::PhasePortrait()
				PhasePortrait end_elem;
				end_elem.InstansiateTrajectories(pVector, startTime);
				end_elem.FollowTrajectories_fixed_time(endTime, *field_ptr.get(), dt, fixed_time);
				end_elem.improve_discretization(endTime, *field_ptr.get(), dt, fixed_time, data);

				return end_elem.create_geos_polygon();
			}

			/*void FollowPhasePortrait(std::map<std::wstring, double> data, double startTime, double endTime, double dt)
			{
				FollowPhasePortrait(PhasePortrait::DistributeStartPoints_radial(data), 
					startTime, endTime, dt);
			}*/

			geos_polygon FollowPhasePortrait_fixed_time(std::map<std::wstring, double> data, double startTime, double endTime, double fixed_time)
			{
				double dt = data.at(L"time_step");
				return FollowPhasePortrait_fixed_time(std::move(PhasePortrait::DistributeStartPoints_radial(data)),
					startTime, endTime, dt, fixed_time, data);
			}

			/*void FollowPhasePortrait(double startTime, double endTime, double dt)
			{
				FollowPhasePortrait(PhasePortrait::DistributeStartPoints_rectangle(field_ptr->porosity().x_mesh(), field_ptr->porosity().y_mesh()),
					startTime, endTime, dt);
			}*/

			/*void FollowPhasePortrait(double startTime, double endTime, double dt, const std::map<std::wstring, double> data)
			{
				FollowPhasePortrait(PhasePortrait::DistributeStartPoints_rectangle(data),
					startTime, endTime, dt);
			}*/

			//const std::vector<reservoir_simulator::phasePortrait::PhasePortrait>& CalculatedPhasePortraits()
			//{
			//	return phasePortraitEnsemble;
			//}

			/*std::wstring PrintPhasePortrait()
			{
				std::wstring result;
				result = std::to_wstring(phasePortraitEnsemble.size()) + L";";
				for (int i = 0; i < phasePortraitEnsemble.size(); i++)
					result += phasePortraitEnsemble[i].PrintPhasePortrait();
				return result;
			}*/
		};
	} // phasePortrait
} // reservoir_simulator