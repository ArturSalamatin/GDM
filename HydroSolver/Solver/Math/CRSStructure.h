#pragma once
#include <vector>
#include <cstddef>

namespace reservoir_simulator
{
	namespace linear_problem
	{
		enum class Layout : unsigned char
		{
			InterleavedSwP,  // [Sw0,P0, Sw1,P1, ...] — current order
			InterleavedPSw,  // [P0,Sw0, P1,Sw1, ...] — CPR-compatible (col%B==0 is P)
			Blocked          // [P0,P1,...,PN, Sw0,Sw1,...,SwN]
		};

		class CRSStructure
		{
		public:
			CRSStructure(Layout layout, unsigned char eqNmbr, size_t cellNmbr,
				const std::vector<std::vector<int>>& connectivityGraph,
				const std::vector<bool>& blockPattern);

			const std::vector<size_t>& Row() const { return row_; }
			const std::vector<size_t>& Col() const { return col_; }
			const std::vector<size_t>& DiagBlocks() const { return diagBlocks_; }
			const std::vector<size_t>& OffDiagBlocks() const { return offDiagBlocks_; }
			const std::vector<size_t>& ElementsAboveBlockRow() const { return elementsAboveBlockRow_; }

			size_t TotalBlocks() const { return totalBlocks_; }
			size_t NnzPerBlock() const { return nnzPerBlock_; }
			unsigned char EqNmbr() const { return eqNmbr_; }
			Layout GetLayout() const { return layout_; }

			size_t GlobalIndex(size_t cell, unsigned char var) const;
			void UnpackCorrections(size_t cell, const double* corrections, double* physical) const;

		private:
			Layout layout_;
			unsigned char eqNmbr_;
			size_t cellNmbr_;
			size_t nnzPerBlock_;
			size_t totalBlocks_;

			std::vector<size_t> row_;
			std::vector<size_t> col_;
			std::vector<size_t> diagBlocks_;
			std::vector<size_t> offDiagBlocks_;
			std::vector<size_t> elementsAboveBlockRow_;

			// physical var index v -> CRS-local var index within a block
			unsigned char permPhysicalToCRS_[4]; // max B=4
			unsigned char permCRSToPhysical_[4];

			// block element (physical row r, physical col c) -> CRS block position
			unsigned char blockPermPhysicalToCRS_[16]; // max blockSize=16

			void buildPermutations();
			void buildInterleaved(size_t cellNmbr,
				const std::vector<std::vector<int>>& connectivityGraph,
				const std::vector<bool>& blockPattern);
			void buildBlocked(size_t cellNmbr,
				const std::vector<std::vector<int>>& connectivityGraph,
				const std::vector<bool>& blockPattern);
		};

	} // linear_problem
} // reservoir_simulator
