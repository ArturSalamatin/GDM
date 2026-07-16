#pragma once
#include <vector>
#include <iomanip>

namespace reservoir_simulator
{
	namespace linear_problem
	{
		class SparsityPattern
		{
		protected:
			//	std::vector<int> neighboursIdx;
			//	std::vector<int> connectivityGraph; // cell indices connected to the current cell l
			std::vector<size_t> blocksPerRow; // including diagonal blocks

			size_t totalNmbrOfBlocks;

			std::vector<bool> blockPattern; // row-wise, nonzero elements if true
			unsigned char eqNmbr; // nmbr of equations per grid cell
			size_t blockSize; // number of non-zero elements in the unit block

#ifdef GDM_DUMP_DEBUG
			// print connectivityGraph to file
			void printPattern();
			// print indices of val-vector with diagonal blocks
			void printDiagonalBlocks();
			// print indices of val-vector with off-diagonal blocks
			void printOffDiagBlocks();
#endif

			std::vector<size_t> row_raw, col_raw; // matrix pattern for cells only. No wells.
			std::vector<size_t> diagBlocks_raw; // indices in value array with elements of diagonal blocks
			std::vector<size_t> offDiagBlocks_raw;  // indices in value array with elements of off-diagonal blocks
			std::vector<size_t> elementsAboveBlockRow; // global offset for the current matrix row 

		public:
			size_t TotalNmbrOfBlocks() const;
			const std::vector<size_t>& Row()  const;
			const std::vector<size_t>& Col()  const;
			size_t NmbrOfNonzerosPerUnitBlock() const;
			unsigned char EqNmbr() const { return eqNmbr; }

			const std::vector<size_t>& OffDiagBlocks()  const;
			const std::vector<size_t>& DiagBlocks()  const;
			const std::vector<size_t>& NmbrOfElementsAboveBlockRow() const;

			SparsityPattern() noexcept;
			SparsityPattern(const unsigned char eqNmbr_, const size_t cellNmbr,
				const std::vector<std::vector<int>>& connectivityGraph, //const std::vector<int>& neighbours, 
				const std::vector<bool>& blPattern);

			SparsityPattern(const unsigned char eqNmbr_, const size_t cellNmbr,
				const std::vector<std::vector<int>>& connectivityGraph);

		private:
			template<typename stream>
			stream& printPattern2Stream(stream& s,
				const std::vector<size_t>& row_raw,
				const std::vector<size_t>& col_raw)
			{
				if (col_raw.empty()) return s;
				size_t width = 2 + (size_t)std::log10(
					*std::max_element(col_raw.begin(), col_raw.end())
					+ 1);

				s << "Sparsity pattern:\n";
				// loop over every cell in mesh
				for (size_t l = 0, idx = 0; l < row_raw.size() - 1; ++l)
				{
					size_t beginNeigbour = row_raw[l], endNeighbour = row_raw[l + 1];
					size_t nmbrNeighours = endNeighbour - beginNeigbour;
					for (size_t i = beginNeigbour; i < endNeighbour; ++i)
					{
						s << std::setw(width) << col_raw[idx] << " "; // idx of non-zero cell in the matrix at line l
						++idx;
					}
					s << std::endl;
				}
				return s;
			}
			template<typename stream>
			stream& printDiagBlocks2Stream(stream& s,
				const std::vector<bool>& blockPattern,
				const std::vector<size_t>& diagBlocks_raw,
				size_t size)
			{
				if (diagBlocks_raw.empty()) return s;
				size_t blockSize = NmbrOfNonzerosPerUnitBlock();
				size_t width = 2 + (size_t)std::log10(
					*std::max_element(diagBlocks_raw.begin(), diagBlocks_raw.end())
					+ 1);

				s << "Diagonal blocks:\n";
				for (size_t l = 0, idx = 0; l < (size - 1) / eqNmbr; ++l)
				{
					for (size_t diagBlockElem = 0; diagBlockElem < blockSize; diagBlockElem++)
					{
						s << std::setw(width) << diagBlocks_raw[idx];
						++idx;
					}
					s << std::endl;
				}
				return s;
			}
			template<typename stream>
			stream& printOffDiagBlocks2Stream(stream& s,
				const std::vector<bool>& blockPattern,
				const std::vector<size_t>& offDiagBlocks_raw)
			{
				if (offDiagBlocks_raw.empty()) return s;
				size_t blockSize = NmbrOfNonzerosPerUnitBlock();
				size_t width = 2 + (size_t)std::log10(
					*std::max_element(offDiagBlocks_raw.begin(), offDiagBlocks_raw.end())
					+ 1);

				s << "Off-diagonal blocks:\n";

				for (size_t l = 0, idx = 0; l < (row_raw.size() - 1) / eqNmbr; ++l)
				{
					for (size_t neibIdx = 0; neibIdx < blocksPerRow[l] - 1; ++neibIdx)
					{
						for (size_t diagBlockElem = 0; diagBlockElem < blockSize; ++diagBlockElem)
						{
							s << std::setw(width) << offDiagBlocks_raw[idx];
							++idx;
						}
						s << "   ";
					}
					s << std::endl;
				}
				return s;
			}
		};
	} // linear_problem
} // reservoir_simulator