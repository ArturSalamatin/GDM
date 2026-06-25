#include "CRSStructure.h"
#include <numeric>
#include <algorithm>

namespace reservoir_simulator
{
	namespace linear_problem
	{

		void CRSStructure::buildPermutations()
		{
			for (unsigned char i = 0; i < eqNmbr_; i++)
			{
				permPhysicalToCRS_[i] = i;
				permCRSToPhysical_[i] = i;
			}

			if (layout_ == Layout::InterleavedPSw)
			{
				permPhysicalToCRS_[0] = 1; // Sw(phys 0) -> CRS slot 1
				permPhysicalToCRS_[1] = 0; // P (phys 1) -> CRS slot 0
				permCRSToPhysical_[0] = 1; // CRS slot 0 -> P (phys 1)
				permCRSToPhysical_[1] = 0; // CRS slot 1 -> Sw(phys 0)
			}

			for (unsigned char r = 0; r < eqNmbr_; r++)
				for (unsigned char c = 0; c < eqNmbr_; c++)
				{
					unsigned char physIdx = r * eqNmbr_ + c;
					unsigned char crsRow = permPhysicalToCRS_[r];
					unsigned char crsCol = permPhysicalToCRS_[c];
					blockPermPhysicalToCRS_[physIdx] = crsRow * eqNmbr_ + crsCol;
				}
		}

		// Build CRS for interleaved layouts (both SwP and PSw).
		// Row/col are built in CRS-row order.
		// diagBlocks_/offDiagBlocks_ are indexed in PHYSICAL block order:
		//   diagBlocks_[cellOffset + physRow * B + physCol] = position in val[]
		// This way CopyBlock(offset, val, data, diagBlocks) with data in physical order
		// writes each element to its correct CRS position.
		void CRSStructure::buildInterleaved(size_t cellNmbr,
			const std::vector<std::vector<int>>& connectivityGraph,
			const std::vector<bool>& blockPattern)
		{
			const unsigned char B = eqNmbr_;

			std::vector<unsigned char> blockPatternRowSize(B, 0);
			for (unsigned char i = 0; i < B; ++i)
				blockPatternRowSize[i] =
				(unsigned char)std::accumulate(blockPattern.begin() + i * B, blockPattern.begin() + (i + 1) * B, 0);

			std::vector<size_t> blocksPerRow(cellNmbr, 1);

			diagBlocks_.reserve(cellNmbr * nnzPerBlock_);
			offDiagBlocks_.reserve(cellNmbr * 6 * nnzPerBlock_);
			col_.reserve(connectivityGraph.size() * 7 * nnzPerBlock_);
			row_.reserve(cellNmbr * B + 1);
			row_.push_back(0);

			// Temporary storage: for each cell, diagBlock positions indexed by physical order
			// diagTmp[physRow * B + physCol] = val[] position
			std::vector<size_t> diagTmp(nnzPerBlock_);
			// offDiagTmp[neighbourIdx][physRow * B + physCol] = val[] position
			std::vector<std::vector<size_t>> offDiagTmp;

			for (size_t l = 0; l < cellNmbr; ++l)
			{
				const std::vector<int>& curNeighbours = connectivityGraph[l];
				size_t nmbrNeighours = curNeighbours.size();
				blocksPerRow[l] = nmbrNeighours + 1;

				std::fill(diagTmp.begin(), diagTmp.end(), 0);
				offDiagTmp.assign(nmbrNeighours, std::vector<size_t>(nnzPerBlock_, 0));

				// Build row_/col_ in CRS-row order: crsRow = 0, 1, ..., B-1
				for (unsigned char crsRow = 0; crsRow < B; crsRow++)
				{
					unsigned char physRow = permCRSToPhysical_[crsRow];

					row_.push_back(row_.back() + (1 + nmbrNeighours) * blockPatternRowSize[physRow]);

					bool f = true;
					for (int neighbourIdx = 0; neighbourIdx < (int)nmbrNeighours; neighbourIdx++)
					{
						if (f && curNeighbours[neighbourIdx] > (int)l)
						{
							for (unsigned char crsCol = 0; crsCol < B; crsCol++)
							{
								unsigned char physCol = permCRSToPhysical_[crsCol];
								if (!blockPattern[B * physRow + physCol])
									continue;
								size_t physBlockIdx = physRow * B + physCol;
								diagTmp[physBlockIdx] = col_.size();
								col_.push_back(l * B + crsCol);
							}
							f = false;
						}

						for (unsigned char crsCol = 0; crsCol < B; crsCol++)
						{
							unsigned char physCol = permCRSToPhysical_[crsCol];
							if (!blockPattern[B * physRow + physCol])
								continue;
							size_t physBlockIdx = physRow * B + physCol;
							offDiagTmp[neighbourIdx][physBlockIdx] = col_.size();
							col_.push_back(curNeighbours[neighbourIdx] * B + crsCol);
						}
					}
					if (f)
					{
						for (unsigned char crsCol = 0; crsCol < B; crsCol++)
						{
							unsigned char physCol = permCRSToPhysical_[crsCol];
							if (!blockPattern[B * physRow + physCol])
								continue;
							size_t physBlockIdx = physRow * B + physCol;
							diagTmp[physBlockIdx] = col_.size();
							col_.push_back(l * B + crsCol);
						}
					}
				}

				// Flush diagTmp to diagBlocks_ in physical order
				for (size_t i = 0; i < nnzPerBlock_; i++)
					diagBlocks_.push_back(diagTmp[i]);

				// Flush offDiagTmp to offDiagBlocks_ in physical order
				for (int neighbourIdx = 0; neighbourIdx < (int)nmbrNeighours; neighbourIdx++)
					for (size_t i = 0; i < nnzPerBlock_; i++)
						offDiagBlocks_.push_back(offDiagTmp[neighbourIdx][i]);
			}

			// Erase zero entries from offDiagBlocks_ (same cleanup as original)
			for (auto it = offDiagBlocks_.begin(); it != offDiagBlocks_.end();)
			{
				if (*it == 0)
					offDiagBlocks_.erase(it);
				else
					++it;
			}

			elementsAboveBlockRow_.resize(cellNmbr, 0);
			for (size_t l = 1; l < cellNmbr; l++)
				elementsAboveBlockRow_[l] = elementsAboveBlockRow_[l - 1] + nnzPerBlock_ * blocksPerRow[l - 1];

			totalBlocks_ = std::accumulate(blocksPerRow.begin(), blocksPerRow.end(), (size_t)0);
		}

		void CRSStructure::buildBlocked(size_t cellNmbr,
			const std::vector<std::vector<int>>& connectivityGraph,
			const std::vector<bool>& blockPattern)
		{
			const unsigned char B = eqNmbr_;

			std::vector<unsigned char> blockPatternRowSize(B, 0);
			for (unsigned char i = 0; i < B; ++i)
				blockPatternRowSize[i] =
				(unsigned char)std::accumulate(blockPattern.begin() + i * B, blockPattern.begin() + (i + 1) * B, 0);

			std::vector<size_t> blocksPerRow(cellNmbr, 1);

			row_.reserve(cellNmbr * B + 1);
			row_.push_back(0);
			diagBlocks_.reserve(cellNmbr * nnzPerBlock_);
			col_.reserve(connectivityGraph.size() * 7 * nnzPerBlock_);

			diagBlocks_.resize(cellNmbr * nnzPerBlock_, 0);

			// Pre-compute neighbour counts and offDiagStart per cell
			std::vector<size_t> nNeib(cellNmbr);
			std::vector<size_t> offDiagStart(cellNmbr, 0);
			size_t totalOffDiag = 0;
			for (size_t l = 0; l < cellNmbr; l++)
			{
				nNeib[l] = connectivityGraph[l].size();
				blocksPerRow[l] = nNeib[l] + 1;
				offDiagStart[l] = totalOffDiag;
				totalOffDiag += nNeib[l] * nnzPerBlock_;
			}
			offDiagBlocks_.resize(totalOffDiag, 0);

			for (unsigned char physRow = 0; physRow < B; physRow++)
			{
				for (size_t l = 0; l < cellNmbr; l++)
				{
					const std::vector<int>& curNeighbours = connectivityGraph[l];
					size_t nmbrNeighours = nNeib[l];

					size_t nnzThisRow = 0;
					for (unsigned char physCol = 0; physCol < B; physCol++)
						if (blockPattern[B * physRow + physCol])
							nnzThisRow += 1 + nmbrNeighours;
					row_.push_back(row_.back() + nnzThisRow);

					for (unsigned char physCol = 0; physCol < B; physCol++)
					{
						if (!blockPattern[B * physRow + physCol])
							continue;

						bool diagInserted = false;
						size_t diagCol = physCol * cellNmbr + l;

						for (int ni = 0; ni < (int)nmbrNeighours; ni++)
						{
							size_t neibCol = physCol * cellNmbr + curNeighbours[ni];
							if (!diagInserted && diagCol < neibCol)
							{
								size_t physBlockIdx = physRow * B + physCol;
								diagBlocks_[l * nnzPerBlock_ + physBlockIdx] = col_.size();
								col_.push_back(diagCol);
								diagInserted = true;
							}
							size_t offIdx = offDiagStart[l] + ni * nnzPerBlock_ + physRow * B + physCol;
							offDiagBlocks_[offIdx] = col_.size();
							col_.push_back(neibCol);
						}
						if (!diagInserted)
						{
							size_t physBlockIdx = physRow * B + physCol;
							diagBlocks_[l * nnzPerBlock_ + physBlockIdx] = col_.size();
							col_.push_back(diagCol);
						}
					}
				}
			}

			elementsAboveBlockRow_.resize(cellNmbr, 0);
			for (size_t l = 1; l < cellNmbr; l++)
				elementsAboveBlockRow_[l] = elementsAboveBlockRow_[l - 1] + nnzPerBlock_ * blocksPerRow[l - 1];

			totalBlocks_ = std::accumulate(blocksPerRow.begin(), blocksPerRow.end(), (size_t)0);
		}

		CRSStructure::CRSStructure(Layout layout, unsigned char eqNmbr, size_t cellNmbr,
			const std::vector<std::vector<int>>& connectivityGraph,
			const std::vector<bool>& blockPattern)
			: layout_(layout), eqNmbr_(eqNmbr), cellNmbr_(cellNmbr),
			nnzPerBlock_(std::accumulate(blockPattern.begin(), blockPattern.end(), (size_t)0)),
			totalBlocks_(0)
		{
			std::fill(std::begin(permPhysicalToCRS_), std::end(permPhysicalToCRS_), 0);
			std::fill(std::begin(permCRSToPhysical_), std::end(permCRSToPhysical_), 0);
			std::fill(std::begin(blockPermPhysicalToCRS_), std::end(blockPermPhysicalToCRS_), 0);

			buildPermutations();

			if (layout == Layout::Blocked)
				buildBlocked(cellNmbr, connectivityGraph, blockPattern);
			else
				buildInterleaved(cellNmbr, connectivityGraph, blockPattern);
		}

		size_t CRSStructure::GlobalIndex(size_t cell, unsigned char var) const
		{
			if (layout_ == Layout::Blocked)
				return (size_t)var * cellNmbr_ + cell;
			else
				return cell * eqNmbr_ + permPhysicalToCRS_[var];
		}

		void CRSStructure::UnpackCorrections(size_t cell, const double* corrections, double* physical) const
		{
			for (unsigned char v = 0; v < eqNmbr_; v++)
				physical[v] = corrections[GlobalIndex(cell, v)];
		}

	} // linear_problem
} // reservoir_simulator
