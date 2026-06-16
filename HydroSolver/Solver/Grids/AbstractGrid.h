#pragma once
#include <fstream>
#include "Cells/AbstractCells.h"
#include "GridDescriptors.h"

namespace reservoir_simulator
{
	using namespace cell;

	namespace grid
	{
		// abstract grid for 
		template<class ProcessCell, class SomeDimCell = Dim3Cell>
		class SomeGrid
		{
		protected:
			std::vector<ProcessCell> Cells;// only active cells
			std::vector<ProcessCell> CellsInactive;// only NOT-active cells

			// only active cells are taken into account
			// cell_idx_Local2Global to get the global idx
			std::vector<std::vector<int>> connectivityGraph; // std::vector of neighbours
			std::vector<std::vector<double>> commonEdgeArea; // corresponding area over the distance

			// inactive cells are always assumed
			std::vector<size_t> cell_idx_Local2Global; // stored values are cell idx in global indexing format, when inactive cells are not counted
			std::vector<long int> cell_idx_Global2Local; // stored values are cell idx in local indexing format, when inactive cells are also counted; some elements are -1
			std::vector<bool> IsCellActive; // shows whether the cell is active or not

			int activeCellsNmbr = 0; // number of active cells; size of connectivityGraph
			int inActiveCellsNmbr = 0; // number of NOTactive cells
			size_t totalCellNmbr = 0; // total number of cells
		public:

			void AcceptState()
			{
//#ifdef	USE_PARALLEL
//#pragma omp parallel for
//#endif
				for (int l = 0; l < ActiveCellsNmbr(); l++)
					Cells[l].AcceptState();
			}
			void ReverseState()
			{
//#ifdef	USE_PARALLEL
//#pragma omp parallel for
//#endif
				for (int l = 0; l < ActiveCellsNmbr(); l++)
					Cells[l].ReverseState();
			}
			void UpdateState(const std::vector<double>& corrections, int eqNmbr)
			{
//#ifdef	USE_PARALLEL
//#pragma omp parallel for
//#endif
				for (int l = 0; l < ActiveCellsNmbr(); l++)
					Cells[l].UpdateState(corrections, l * eqNmbr);
			}

			const std::vector<ProcessCell*> GetNeighboursPointer(int l)
			{
				std::vector<ProcessCell*> neighbours;
				for (int neighbourIdx = 0; neighbourIdx < connectivityGraph[l].size(); neighbourIdx++)
				{

					neighbours.push_back(&(Cells[connectivityGraph[l][neighbourIdx]]));
				}

				return neighbours;
			}

			const std::vector<double>& CommonEdgeArea(int l)
			{
				return commonEdgeArea[l];
			}
			// returns the cell according to its local index
			// idx < 0 stands for inactive cells
			const ProcessCell& operator [] (int idx) const
			{
				if (idx < 0)
				{
					return CellsInactive[-(idx + 1)];
				}
				else
				{
					return Cells[idx];
				}
			}

			/*	const std::vector<ProcessCell>& GetActiveCells()
				{
					return Cells;
				}

				const std::vector<ProcessCell>& GetInActiveCells()
				{
					return CellsInactive;
				}*/

			long int ConvertGlobal2Local(size_t idx) const
			{
				return cell_idx_Global2Local[idx];
			}

			const std::vector<int>& GetNeighboursIdx(int l)
			{
				return connectivityGraph[l];
			}
			const std::vector<double>& GetAreas(int l)
			{
				return commonEdgeArea[l];
			}
			const std::vector<std::vector<int>>& GetConnectivityGraph()
			{
				return connectivityGraph;
			}

			size_t ActiveCellsNmbr() const { return activeCellsNmbr; }
	//		size_t ActiveCellsNmbr() { return activeCellsNmbr; }
			size_t TotalCellsNmbr() const { return totalCellNmbr; }

			SomeGrid()
				:IsCellActive{  },
				cell_idx_Global2Local{  }, cell_idx_Local2Global{}, connectivityGraph{}, commonEdgeArea{}, Cells{}, CellsInactive{}
			{}
			SomeGrid(const std::vector<bool>& active_cells)
				:Cells{}, CellsInactive{}, IsCellActive{ active_cells },
				cell_idx_Global2Local{ std::vector<long int>(totalCellNmbr, -1) }
			{
				totalCellNmbr = active_cells.size();
				cell_idx_Local2Global.reserve(totalCellNmbr);
				/* loop through every cell, and assemble the
				 * local/global indices vectors */
				for (int l = 0, l0 = -1; l < totalCellNmbr; l++)
				{
					if ((active_cells[l]))
					{
						cell_idx_Local2Global.push_back(l);
						cell_idx_Global2Local[l] = activeCellsNmbr;
						activeCellsNmbr++;
					}
					else
					{
						inActiveCellsNmbr++;
						cell_idx_Global2Local[l] = -inActiveCellsNmbr;
					}
				}
				cell_idx_Local2Global.shrink_to_fit();
			}

			~SomeGrid() {}

			
		};

		template<class ProcessCell>
		class SomeStructuredGrid3Dim : public SomeGrid<ProcessCell, Dim3Cell>
		{
			typedef  SomeGrid<ProcessCell, Dim3Cell> base;
			using base::activeCellsNmbr;
			using base::connectivityGraph;
			using base::commonEdgeArea;
			using base::cell_idx_Global2Local;
			using base::Cells;
			using base::CellsInactive;
		//	using base::ActiveCellsNmbr;

		protected:
			GridSize grid_size; // nmbr of cell in every direction (active + inactive)

		public:
			using base::ConvertGlobal2Local;

			size_t Nx() const { return grid_size.Nx; }
			size_t Ny() const { return grid_size.Ny; }
			size_t Nz() const { return grid_size.Nz; }

			size_t ConvertTriple2Local(const std::vector<size_t>& idx) const
			{
				return ConvertGlobal2Local(Nx() * Ny() * idx[2] + Nx() * idx[1] + idx[0]);
			}
			size_t ConvertTriple2Global(const std::vector<size_t>& idx) const
			{
				return (Nx() * Ny() * idx[2] + Nx() * idx[1] + idx[0]);
			}

			SomeStructuredGrid3Dim() {}

			SomeStructuredGrid3Dim(
				std::vector<ProcessCell>&& cells, 
				std::vector<ProcessCell>&& cellsInactive, 
				const std::vector<bool>& active_cells,
				GridSize grid_size) : 
				SomeGrid<ProcessCell, Dim3Cell>(active_cells),
				grid_size{grid_size}
			{
				Cells = std::move(cells);
				CellsInactive = std::move(cellsInactive);
				SetConnectivityGraph_3D(active_cells);
				printConnectivity();
			}

			void SetConnectivityGraph_3D(const std::vector<bool>& active_cells)
			{
				int N = activeCellsNmbr;
			//	connectivityGraph.clear();
				connectivityGraph.reserve(N);
			//	commonEdgeArea.clear(); 
				commonEdgeArea.reserve(N);

				for (size_t k = 0, l = 0; k < Nz(); ++k)
				{
					for (size_t j = 0; j < Ny(); ++j)
					{
						for (size_t i = 0; i < Nx(); ++i, ++l)
						{
							// loop through every cell and determine its neighbours
							std::vector<int> connections;
							std::vector<double> faces;
							if (active_cells[l]) // current cell is active
							{// check the neighbour cells, whether they are active and flux should be taken into account
								//if (k > 0 && active_cells[l - Nx() * Ny()])
								//{// the cell, previous in Z direction
								//	connections.push_back(cell_idx_Global2Local[l - Nx() * Ny()]);
								//	faces.push_back(Cells[cell_idx_Global2Local[l]].StepX() *
								//		Cells[cell_idx_Global2Local[l]].StepY() *
								//		2 / (Cells[cell_idx_Global2Local[l - Nx() * Ny()]].StepZ() + Cells[cell_idx_Global2Local[l]].StepZ()));
								//}
								if (j > 0 && active_cells[l - Nx()])
								{// the cell, previous in Y direction
									connections.push_back(cell_idx_Global2Local[l - Nx()]);
									faces.push_back(Cells[cell_idx_Global2Local[l]].StepX() /
										Cells[cell_idx_Global2Local[l]].StepY() *
										(Cells[cell_idx_Global2Local[l - Nx()]].StepZ() + Cells[cell_idx_Global2Local[l]].StepZ()) / 2);
								}
								if (i > 0 && active_cells[l - 1])
								{// the cell, previous in X direction
									connections.push_back(cell_idx_Global2Local[l - 1]);
									faces.push_back(Cells[cell_idx_Global2Local[l]].StepY() /
										Cells[cell_idx_Global2Local[l]].StepX() *
										(Cells[cell_idx_Global2Local[l - 1]].StepZ() + Cells[cell_idx_Global2Local[l]].StepZ()) / 2);
								}
								if (i < Nx() - 1 && active_cells[l + 1])
								{// the cell, following in X direction
									connections.push_back(cell_idx_Global2Local[l + 1]);
									faces.push_back(Cells[cell_idx_Global2Local[l]].StepY() /
										Cells[cell_idx_Global2Local[l]].StepX() *
										(Cells[cell_idx_Global2Local[l + 1]].StepZ() + Cells[cell_idx_Global2Local[l]].StepZ()) / 2);
								}
								if (j < Ny() - 1 && active_cells[l + Nx()])
								{// the cell, following in Y direction
									connections.push_back(cell_idx_Global2Local[l + Nx()]);
									faces.push_back(Cells[cell_idx_Global2Local[l]].StepX() /
										Cells[cell_idx_Global2Local[l]].StepY() *
										(Cells[cell_idx_Global2Local[l + Nx()]].StepZ() + Cells[cell_idx_Global2Local[l]].StepZ()) / 2);
								}
								//if (k < Nz() - 1 && active_cells[l + Nx() * Ny()])
								//{// the cell, following in Z direction
								//	connections.push_back(cell_idx_Global2Local[l + Nx() * Ny()]);
								//	faces.push_back(Cells[cell_idx_Global2Local[l]].StepX() *
								//		Cells[cell_idx_Global2Local[l]].StepY() *
								//		2 / (Cells[cell_idx_Global2Local[l + Nx() * Ny()]].StepZ() + Cells[cell_idx_Global2Local[l]].StepZ()));
								//}
								connectivityGraph.push_back(connections);
								commonEdgeArea.push_back(faces);
							}
						}
					}
				}
			}
			// print connectivityGraph to file
			void printConnectivity()
			{
				std::ofstream myfile;
				myfile.open("test_connections.txt");

				for (int l = 0; l < activeCellsNmbr; l++)
				{
					for (auto neighbourId : connectivityGraph[l])
					{
						myfile << neighbourId << " ";
					}
					myfile << std::endl;
				}
			}

			const ProcessCell& operator() (size_t i, size_t j, size_t k) const
			{
				size_t idx = ConvertTriple2Local(std::vector<size_t>{i, j, k});
				return (*this)[idx];
			}
		};
	} // grid
} // reservoir_simulator