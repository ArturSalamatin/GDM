#include "SomeFlowField.h"

namespace reservoir_simulator
{
	namespace phasePortrait
	{   
		std::vector<double> create_uniform_mesh(double h, int n, double start)
		{
			std::vector<double> mesh;
			mesh.push_back(start);
			for (int i = 1; i < n; i++)
				mesh.push_back(mesh.back() + h);
			return mesh;
		}

		std::vector<std::vector<double>> create_uniform_mesh_2D(double hx, int nx, double startx, double hy, int ny, double starty)
		{
			std::vector<std::vector<double>> mesh;
			mesh.emplace_back(std::move(create_uniform_mesh(hx, nx, startx)));
			mesh.emplace_back(std::move(create_uniform_mesh(hy, ny, starty)));
			return mesh;
		}

		const std::string SomeField2D::print() const
		{
			std::string result;
			for (int j = 0; j < field.size(); j++)
				for (int i = 0; i < field[j].size(); i++)
				{
					char buffer[40];
					snprintf(buffer, 40, "%+19.11E;", field[j][i]);
					result += buffer;
				}
			return result;
		}

		void SomeField2D::write(std::ofstream& wstream)
		{
			for (int j = 0, l = 0; j < field.size(); j++)
				wstream.write(reinterpret_cast<const char*>(&field[j][0]), field[j].size() * sizeof(double));
		}

		double FlowFieldSnapshot::V(const phasePortrait::Point& P)
		{
			auto vxvy = (*this)(P);
			return sqrt(vxvy.first * vxvy.first + vxvy.second * vxvy.second);
		}

		const std::string FlowFieldSnapshot::print() const
		{
			char buffer[40];
			snprintf(buffer, 40, "%+19.11E;", time);
			std::string result = buffer;

			result += vxField.print() + vyField.print();
			return result;
		}

		void FlowFieldSnapshot::write(std::ofstream& wstream)
		{
			wstream.write(reinterpret_cast<const char*>(&time), sizeof(time));
			vxField.write(wstream);
			vyField.write(wstream);
		}

		std::pair<double, double> SomeFlowField::operator()(double queryT, const phasePortrait::Point& queryP)
		{
			auto lowT_idx = math_routines::MathRoutines::LowerPointNonUniformMesh(time, queryT);
			if (lowT_idx < 0)
				return {0.0, 0.0};
			if (lowT_idx == time.size() - 1)
				lowT_idx--;
			auto upT_idx = lowT_idx + 1;
			auto lowT = time[lowT_idx], upT = time[upT_idx];

			const auto& lowV = field[lowT_idx](queryP);
			const auto& upV = field[upT_idx](queryP);

			const auto m = poro(queryP);

			auto result = math_routines::MathRoutines::LinearInterp(queryT, lowT, upT, lowV, upV);
			result.first *= multiplier / m;
			result.second *= multiplier / m;

			return result;
		}

		const std::string
			SomeFlowField::print() const
		{
			char buffer[40];
			snprintf(buffer, 40, "%u;", field.size());
			std::string result = buffer;

			for (int t = 0; t < field.size(); t++)
			{
				result += field[t].print();
			}
			return result;
		}


		void SomeFlowField::write(std::ofstream& wstream)
		{
			int n = field.size();
			wstream.write(reinterpret_cast<const char*>(&n), sizeof(n));
			for (int t = 0; t < field.size(); t++)
				field[t].write(wstream);
		}
	}
}