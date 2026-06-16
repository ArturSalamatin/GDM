#pragma once
#include "../../stdafx.h"

#include "Point.h"
#include "../../Solver/Math/MathRoutines.h"

namespace reservoir_simulator
{
	namespace phasePortrait
	{
		std::vector<double> create_uniform_mesh(double h, int n, double start);
		std::vector<std::vector<double>> create_uniform_mesh_2D(double hx, int nx, double startx, double hy, int ny, double starty);

		class Mesh_1D
		{
		protected:
			std::vector<std::vector<double>> mesh;
		public:
			const std::vector<double>& GetXmesh() const { return mesh[0]; }

			Mesh_1D(Mesh_1D&&) noexcept = default;
			Mesh_1D(std::vector<std::vector<double>>&& mesh_) noexcept
				: mesh{ std::move(mesh_) } {};
			Mesh_1D(const Mesh_1D&) = default;
		};

		class Mesh_2D : public Mesh_1D
		{
		public:
			const std::vector<double>& GetYmesh() const { return mesh[1]; }
			Mesh_2D(Mesh_2D&&) noexcept = default;
			Mesh_2D(const Mesh_2D&)  = default;
			Mesh_2D(std::vector<std::vector<double>> && mesh_) noexcept
				: Mesh_1D{std::move(mesh_)} {};
		};

		class SomeField2D
		{
		protected:
			Mesh_2D mesh;
			std::vector<std::vector<double>> field;		

		public:
		//	SomeField2D() {}
			SomeField2D(const SomeField2D&) = default;
			SomeField2D(std::vector<std::vector<double>>&& v, Mesh_2D&& mesh_) //noexcept
				:field{ std::move(v) }, mesh{std::move(mesh_)}
			{}
			double operator()(const phasePortrait::Point& queryP)
			{
				return math_routines::MathRoutines::InterpFieldConstTime(queryP, x_mesh(), y_mesh(), field);
			}
			double operator()(double x, double y) { return (*this)(Point(x, y)); }

			const std::vector<double>& 
				x_mesh() const { return mesh.GetXmesh(); }
			const std::vector<double>& 
				y_mesh() const { return mesh.GetYmesh(); }

			const std::vector<std::vector<double>>& 
				get_field() const { return field; }

			const std::wstring print() const;

			void write(std::ofstream& wstream);
		};

		class PorosityField : public SomeField2D
		{
		public:
			PorosityField(const PorosityField&) = default;
			PorosityField(double x0, int nx, double hx, double y0, int ny, double hy, std::vector<std::vector<double>>&& porosity)
				: SomeField2D{ std::move(porosity), std::move(create_uniform_mesh_2D(hx, nx, x0 + hx / 2.0, hy, ny, y0 + hy / 2.0)) }
			{}
		};

		// vx[y][x]
		class FlowFieldComponentX : public SomeField2D
		{
		public:
			FlowFieldComponentX(double x0, int nx, double hx, double y0, int ny, double hy, std::vector<std::vector<double>>&& v)
				: SomeField2D{ std::move(v), std::move(create_uniform_mesh_2D(hx, nx + 1, x0, hy, ny, y0 + hy / 2.0)) }
			{}
		};
		// vy[y][x]
		class FlowFieldComponentY : public SomeField2D
		{
		public:
			FlowFieldComponentY(double x0, int nx, double hx, double y0, int ny, double hy, std::vector<std::vector<double>>&& v)
				: SomeField2D{ std::move(v), std::move(create_uniform_mesh_2D(hx, nx, x0 + hx / 2.0, hy, ny + 1, y0)) }
			{}
		};

		class FlowFieldSnapshot
		{
		protected:
			double time;
			FlowFieldComponentX vxField;
			FlowFieldComponentY vyField;

		public:
			double t() { return time; }
			double V(double x, double y) { return V(phasePortrait::Point(x, y)); }
			double V(const phasePortrait::Point& P);
			FlowFieldSnapshot(double t_,
				double x0, int nx, double hx,
				double y0, int ny, double hy,
				std::vector<std::vector<double>>&& vx_,
				std::vector<std::vector<double>>&& vy_)
				: time{ t_ }, vxField{ x0, nx, hx, y0, ny, hy ,std::move(vx_) }, vyField{ x0, nx, hx, y0, ny, hy, std::move(vy_) }
			{}

			std::pair<double, double> 
				operator()(const reservoir_simulator::phasePortrait::Point& queryP){
				return std::make_pair(vxField ( queryP), vyField(queryP));
			}
			std::pair<double, double> 
				operator()(double x, double y) { return (*this)(reservoir_simulator::phasePortrait::Point(x, y)); }

			const FlowFieldComponentX& get_vxField() const { return vxField; }
			const FlowFieldComponentY& get_vyField() const { return vyField; }

			const std::wstring print() const;

			void write(std::ofstream& wstream);
		};

		class SomeFlowField
		{
		public:
			using Ptr = std::shared_ptr<SomeFlowField>;
			using SequencePtr = std::vector<Ptr>;

		protected:
			double x0, y0; // mesh corner, origin
			int nx, ny; // number of cells in every direction
			double hx, hy; // uniform steps in every diretction

			std::vector<FlowFieldSnapshot> field;
			PorosityField poro;
			std::vector<double> time;

			mutable double multiplier = 1.0;
		public:
			void clear() { field.clear(); }
			void add_snapshot(double t, std::vector<std::vector<double>>&& vx, std::vector<std::vector<double>>&& vy)
			{
				time.push_back(t);
				field.emplace_back(t, x0, nx, hx, y0, ny, hy, std::move(vx), std::move(vy));
			}

			SomeFlowField(double x0_, int nx_, double hx_, double y0_, int ny_, double hy_,
				std::vector<std::vector<double>>&& poro_) :x0{ x0_ }, y0{ y0_ }, nx{ nx_ }, ny{ ny_ }, hx{ hx_ }, hy{ hy_ }, 
				poro{ x0, nx, hx, y0, ny, hy, std::move(poro_) }{ }

			// interpolate velocity components
			std::pair<double, double> operator()(double queryT, const phasePortrait::Point&);
			std::pair<double, double> 
				operator()(double queryT, double x, double y){
				return (*this)(queryT, phasePortrait::Point(x, y));
			}

			void SetMutiplier(double val) const { if (val > 0.0) multiplier = val; }

			// interpolate velocity components and calculate std::vector absolute value
			double V(double queryT, const reservoir_simulator::phasePortrait::Point& queryP)
			{
				auto vxvy = (*this)(queryT, queryP);
				return sqrt(vxvy.first * vxvy.first + vxvy.second * vxvy.second);
			}
			double V(double t, double x, double y) { return V(t, reservoir_simulator::phasePortrait::Point(x, y)); }

			const std::vector<FlowFieldSnapshot>& 
				GetVelocityField() const { return field; }
			const PorosityField& 
				porosity() const { return poro; }
			const std::wstring
				print() const;
			void write(std::ofstream& wstream);
			bool is_empty() const { return field.empty(); }
		};
	} // phasePortrait
} // reservoir_simulator







