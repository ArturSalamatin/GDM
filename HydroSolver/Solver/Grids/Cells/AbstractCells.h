#pragma once

#include <vector>
#include <array>
#include <algorithm>


namespace reservoir_simulator
{
	namespace cell
	{
		//interface for a multidimensional cell
		class SomeDimCell
		{
		protected:
			std::vector<double> Center;
			std::vector<double> Size;
			double volume;
		public:
			double Volume() const
			{
				return volume;
			}
			size_t Dim()
			{
				return Size.size();
			}
			SomeDimCell()
				:Center{}, Size{}, volume{ 0 } {};
			SomeDimCell(const std::vector<double>& center, const std::vector<double>& size)
				:Center{ center }, Size{ size }, volume{ size[0] }
			{
				//	Center = center;
			//	Center.shrink_to_fit();
				//	Size = size;
			//	Size.shrink_to_fit();

				//	volume = Size[0];
				for (int i = 1; i < Size.size(); i++)
					volume *= Size[i];
			}
		//	virtual double* D(SomeDimCell& cell) = 0;
		//	virtual double* DerivativeD(SomeDimCell& cell) = 0;
		};

		class Dim1Cell : public SomeDimCell
		{
		public:
			double X() const
			{
				return Center[0];
			}
			double StepX() const
			{
				return Size[0];
			}

			Dim1Cell() {};
			Dim1Cell(const std::vector<double>& center, const std::vector<double>& size) :SomeDimCell(center, size)
			{
				//	assert(center.size() < 1);
				//	assert(size.size() < 1);
			}
			~Dim1Cell() {};
		};

		class Dim2Cell :
			public Dim1Cell
		{
		public:
			double Y() const
			{
				return Center[1];
			}
			double StepY() const
			{
				return Size[1];
			}

			Dim2Cell() {};
			Dim2Cell(const std::vector<double>& center, const std::vector<double>& size) : Dim1Cell(center, size)
			{
				//	assert(center.size() < 2);
				//	assert(size.size() < 2);
			}
			~Dim2Cell() {};
		};

		class Dim3Cell :
			public Dim2Cell
		{

		public:
			double Z() const
			{
				return Center[2];
			}
			/// <summary>
			/// cell size in Z-direction
			/// </summary>
			/// <returns></returns>
			double StepZ() const
			{
				return Size[2];
			}

			Dim3Cell() {};
			Dim3Cell(const std::vector<double>& center, const std::vector<double>& size) : Dim2Cell(center, size)
			{
				//	assert(center.size() < 3);
				//	assert(size.size() < 3);
			}
			~Dim3Cell() {};
		};

		class TimeDependentCell
		{
		protected:
			std::vector<double> PreviousState_VariableFieldProperties;
			std::vector<double> PreviousState_DependentFieldProperties;

			virtual void SetPreviousStateDependentFieldProperties() = 0;

		public:
			TimeDependentCell()
				:PreviousState_VariableFieldProperties{}, PreviousState_DependentFieldProperties{} {}
			TimeDependentCell(const std::vector<double>& initState) 
				:PreviousState_DependentFieldProperties{}, PreviousState_VariableFieldProperties{ initState }
			{
				PreviousState_VariableFieldProperties.shrink_to_fit();
			}
		protected:
			const std::vector<double>& GetPreviousState_DependentFieldProperties() const
			{
				return PreviousState_DependentFieldProperties;
			}
		};

		class PhysPropCell
		{
		public:
			static void set_constantPointProperties(std::array<double, 8> const_point_prop)
			{
				ConstantPointProperties = const_point_prop;
			}
		protected:
			// common properties for every cell (like acceleration of gravity and viscosities of fluids at ambient conditions)
			static std::array<double, 8> ConstantPointProperties;
			// field properties (like S, P, k, m...)
			std::vector<double> ConstantFieldProperties, VariableFieldProperties;
			std::array<double, 10> DependentFieldProperties;

			virtual void UpdateDependentFieldProperties() = 0;
			virtual void ApplyPhysicalConstraints() = 0;

		public:

			const std::vector<double>& GetVariableFieldProperties() const
			{
				return VariableFieldProperties;
			}
			static std::array<double, 8> GetConstPointProperties()
			{
				return ConstantPointProperties;
			}

			PhysPropCell()
				:ConstantFieldProperties{}, VariableFieldProperties{}, DependentFieldProperties{}{}
			PhysPropCell(const std::vector<double>& constFieldProp, const std::vector<double>& varFieldProp)
				:DependentFieldProperties{}, ConstantFieldProperties{ constFieldProp }, VariableFieldProperties{ varFieldProp }
			{
				ConstantFieldProperties.shrink_to_fit();
				VariableFieldProperties.shrink_to_fit();
			}
		};

		// interface for the cell that represents some process
		// values of std::vector-elements are to be identified later
		template<class SomeDimCell = Dim3Cell>
		class SomeProcessCell_TimeDependent : public SomeDimCell, public TimeDependentCell, public PhysPropCell
		{
		public:
			SomeProcessCell_TimeDependent() {}
			SomeProcessCell_TimeDependent(const std::vector<double>& center, const std::vector<double>& size,
				const std::vector<double>& constFieldProp, const std::vector<double>& varFieldProp
			) :
				SomeDimCell(center, size), 
				PhysPropCell(constFieldProp, varFieldProp),
				TimeDependentCell(varFieldProp) {}

			void AcceptState()
			{
				// in case we want to reverse the accepted state
				PreviousState_VariableFieldProperties = VariableFieldProperties;
				SetPreviousStateDependentFieldProperties();
			}

			void ReverseState()
			{
				VariableFieldProperties = PreviousState_VariableFieldProperties;
				UpdateDependentFieldProperties();
			}

			void UpdateState(const std::vector<double>& corrections, int begin)
			{
				// only sets raw data.
				// if projection is required to satisfy 
				// additional constraints like 0 < S < 1
				std::transform(VariableFieldProperties.begin(), VariableFieldProperties.end(), corrections.begin() + begin,
					VariableFieldProperties.begin(), std::plus<double>());

				// then modify this VIRTUAL method below
				UpdateDependentFieldProperties();
			}
		};
	}// cell
}// reservoir_simulator