
#include <numeric>
#include <fstream>
#include "SparsityPattern.h"

namespace reservoir_simulator
{
	namespace linear_problem
	{


		/////////////////// SparsityPattern
		// print connectivityGraph to file
		void SparsityPattern::printPattern()
		{
			std::ofstream myfile{ "test_SparsityPattern.txt" };
			printPattern2Stream(myfile, row_raw, col_raw);
			myfile.close();
		}

		// print indices of val-vector with diagonal blocks
		void SparsityPattern::printDiagonalBlocks()
		{
			std::ofstream myfile{ "test_SparsityPattern_DiagBlocks.txt" };
			printDiagBlocks2Stream(myfile, blockPattern, diagBlocks_raw, row_raw.size());
			myfile.close();
		}

		// print indices of val-vector with off-diagonal blocks
		void SparsityPattern::printOffDiagBlocks()
		{
			std::ofstream myfile{ "test_SparsityPattern_OffDiagBlocks.txt" };
			printOffDiagBlocks2Stream(myfile, blockPattern, offDiagBlocks_raw);
			myfile.close();
		}

		size_t SparsityPattern::TotalNmbrOfBlocks() const { return totalNmbrOfBlocks; }

		const std::vector<size_t>& SparsityPattern::Row() const { return row_raw; }

		const std::vector<size_t>& SparsityPattern::Col() const { return col_raw; }

		size_t SparsityPattern::NmbrOfNonzerosPerUnitBlock() const
		{
			return
				std::accumulate(blockPattern.begin(), blockPattern.end(), 0);
		}

		const std::vector<size_t>& SparsityPattern::OffDiagBlocks() const { return offDiagBlocks_raw; }

		const std::vector<size_t>& SparsityPattern::DiagBlocks() const { return diagBlocks_raw; }

		const std::vector<size_t>& SparsityPattern::NmbrOfElementsAboveBlockRow() const { return elementsAboveBlockRow; }

		SparsityPattern::SparsityPattern() noexcept = default;

		SparsityPattern::SparsityPattern(
			const unsigned char eqNmbr, const size_t cellNmbr,
			const std::vector<std::vector<int>>& connectivityGraph,
			const std::vector<bool>& blPattern) :
			eqNmbr{ eqNmbr },
			blockPattern{ blPattern }, // row-wise block template. Elements are true if block values may be nonzero
			blocksPerRow{ std::vector<size_t>(cellNmbr, 1) }, // there is at least diagonal block in the row
			blockSize{ blPattern.size() } // the product eqNmbr*eqNmbr
		{
			// number of non-zeros in each row of the block
			std::vector<unsigned char> blockPatternRowSize(eqNmbr, 0);
			for (unsigned char i = 0; i < eqNmbr; ++i)
				// loop through block rows
				blockPatternRowSize[i] =
				std::accumulate(blockPattern.begin() + i * eqNmbr, blockPattern.begin() + (i + 1) * eqNmbr, 0);

			diagBlocks_raw.reserve(cellNmbr* blockSize);
			offDiagBlocks_raw.reserve(cellNmbr * 6 * blockSize); // up to 6 neighbours in 3D
			col_raw.reserve((connectivityGraph.size() * 7 //+ cellNmbr
				)* blockSize);
			row_raw.reserve(cellNmbr* eqNmbr + 1);
			row_raw.push_back(0);
			//loop through grid cells
			for (size_t l = 0; l < cellNmbr; ++l)
			{
				// neighbours range
				const std::vector<int>& curNeighbours = connectivityGraph[l];
				size_t nmbrNeighours = curNeighbours.size();
				blocksPerRow[l] = nmbrNeighours + 1;
				// number of elements that already filled in
				// now we fill in elements corresponding to the cell l and its neighbours
				size_t globalOffset = offDiagBlocks_raw.size();
				offDiagBlocks_raw.resize(offDiagBlocks_raw.size() + nmbrNeighours * blockSize);
				for (int blockPatternRow = 0; blockPatternRow < eqNmbr; blockPatternRow++)
				{
					// add row data
					row_raw.push_back(row_raw.back() + (1 + nmbrNeighours) * blockPatternRowSize[blockPatternRow]);
					// add col data
					bool f = true; // must still add the cell itself
					for (int neighbourIdx = 0; neighbourIdx < nmbrNeighours; neighbourIdx++)
					{
						std::vector<size_t> ofDiagElemIdx;
						ofDiagElemIdx.clear();
						if (f && curNeighbours[neighbourIdx] > l)
						{// add the cell
							for (int eqI = 0; eqI < eqNmbr; eqI++) {
								if (blockPattern[eqNmbr * blockPatternRow + eqI] == false)
									continue;// there is no corresponding element in the block pattern, then skip the loop-iteration
								diagBlocks_raw.push_back(col_raw.size());
								col_raw.push_back(l * eqNmbr + eqI);
							}
							f = false;
						}
						// fill in the cell neighbour
						for (int blockPatternCol = 0; blockPatternCol < eqNmbr; blockPatternCol++) {
							if (blockPattern[eqNmbr * blockPatternRow + blockPatternCol] == false)
								continue;// there is no corresponding element in the block pattern, then skip the loop-iteration
							ofDiagElemIdx.push_back(col_raw.size());
							col_raw.push_back(curNeighbours[neighbourIdx] * eqNmbr + blockPatternCol);
						}
						// fill in the off-diagonal indices in nonDiagBlocks
						for (int j = 0, skip = 0; j < eqNmbr; j++)
						{
							if (blockPattern[eqNmbr * blockPatternRow + j] == false)
							{
								skip++;
								continue;// there is no corresponding element in the block pattern, then skip the loop-iteration
							}
							offDiagBlocks_raw[globalOffset + neighbourIdx * blockSize + blockPatternRow * eqNmbr + j - skip] =
								ofDiagElemIdx[j - skip];
						}
					}
					if (f)
					{// the cell was not added. Add it now
						for (int eqI = 0; eqI < eqNmbr; eqI++) {
							if (blockPattern[eqNmbr * blockPatternRow + eqI] == false)
								continue;// there is no corresponding element in the block pattern, then skip the loop-iteration
							diagBlocks_raw.push_back(col_raw.size());
							col_raw.push_back(l * eqNmbr + eqI);
						}
					}
				}
			}

			std::erase(offDiagBlocks_raw, static_cast<size_t>(0));

			elementsAboveBlockRow = std::vector<size_t>(cellNmbr, 0);
			for (size_t l = 1; l < cellNmbr; l++)
				elementsAboveBlockRow[l] = elementsAboveBlockRow[l - 1] + NmbrOfNonzerosPerUnitBlock() * blocksPerRow[l - 1];

			// total number of blocks in the matrix
			totalNmbrOfBlocks = std::accumulate(blocksPerRow.begin(), blocksPerRow.end(), 0);
		}

		SparsityPattern::SparsityPattern(const unsigned char eqNmbr_, const int cellNmbr, const std::vector<std::vector<int>>& connectivityGraph) :
			SparsityPattern(eqNmbr_, cellNmbr,
				connectivityGraph, std::vector<bool>(true, eqNmbr_* eqNmbr_)) {}

		// number of non-zero elements in the matrix


	} // linear_problem
} // reservoir_simulator
