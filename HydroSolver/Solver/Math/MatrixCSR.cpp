#include "MatrixCSR.h"
#include "CRSStructure.h"
#ifdef GDM_DUMP_DEBUG
#include <fstream>
#include "../../Helpers/DebugDump.h"
#endif

namespace reservoir_simulator
{
	namespace linear_problem
	{

		unsigned char MatrixCSR::EqNmbr() const { return crs_->EqNmbr(); }

		/////////////////// MatrixCSR
		void MatrixCSR::CopyBlock(size_t valueOffset, std::vector<double>& dest, const std::vector<double>& data, const std::vector<size_t>& blockPosInValArray)
		{
			for (size_t i = 0; i < NmbrOfNonZerosPerUnitBlock(); i++)
				dest[blockPosInValArray[valueOffset + i]] += data[i];
		}

		void MatrixCSR::CopyBlock(size_t valueOffset, std::vector<double>& dest, const double* data, const std::vector<size_t>& blockPosInValArray)
		{
			for (size_t i = 0; i < NmbrOfNonZerosPerUnitBlock(); i++)
				dest[blockPosInValArray[valueOffset + i]] += data[i];
		}

		size_t MatrixCSR::NmbrOfNonZerosPerUnitBlock() const { return crs_->NnzPerBlock(); }

		const std::vector<size_t>& MatrixCSR::Row() const { return crs_->Row(); }

		const std::vector<size_t>& MatrixCSR::Col() const { return crs_->Col(); }

		const std::vector<double>& MatrixCSR::Val() const { return value; }
		std::vector<double>& MatrixCSR::Val() { return value; }

		void MatrixCSR::AddDiagBlock(size_t l, const std::vector<double>& data)
		{
			size_t valueOffset = l * NmbrOfNonZerosPerUnitBlock();
			CopyBlock(valueOffset, value, data, crs_->DiagBlocks());
		}

		void MatrixCSR::AddOffDiagBlock(size_t l, size_t neibIdx, const std::vector<double>& data)
		{
			size_t valueOffset = crs_->ElementsAboveBlockRow()[l] + (neibIdx - l) * NmbrOfNonZerosPerUnitBlock();
			CopyBlock(valueOffset, value, data, crs_->OffDiagBlocks());
		}

		void MatrixCSR::AddDiagBlock(size_t l, const double* data)
		{
			size_t valueOffset = l * NmbrOfNonZerosPerUnitBlock();
			CopyBlock(valueOffset, value, data, crs_->DiagBlocks());
		}

		void MatrixCSR::AddOffDiagBlock(size_t l, size_t neibIdx, const double* data)
		{
			size_t valueOffset = crs_->ElementsAboveBlockRow()[l] + (neibIdx - l) * NmbrOfNonZerosPerUnitBlock();
			CopyBlock(valueOffset, value, data, crs_->OffDiagBlocks());
		}

		MatrixCSR::MatrixCSR() noexcept = default;

		MatrixCSR::MatrixCSR(
			Layout layout, unsigned char eqNmbr_, size_t cellNmbr,
			const std::vector<std::vector<int>>& connectivityGraph,
			const std::vector<bool>& blPattern) noexcept :
			crs_{ std::make_unique<CRSStructure>(layout, eqNmbr_, cellNmbr, connectivityGraph, blPattern) },
			nnz{ crs_->TotalBlocks() * crs_->NnzPerBlock() }
		{
			value.resize(nnz, 0.0);
		}

		MatrixCSR::~MatrixCSR() = default;

		void MatrixCSR::ResetMatrix()
		{
			std::fill(value.begin(), value.end(), 0.0);
		}

		std::vector<std::vector<double>> MatrixCSR::toDense() const
		{
			size_t N = Row().size() - 1;
			std::vector<std::vector<double>> dense(N, std::vector<double>(N, 0.0));
			for (size_t i = 0; i < N; i++)
				for (size_t k = Row()[i]; k < Row()[i + 1]; k++)
					dense[i][Col()[k]] = Val()[k];
			return dense;
		}

#ifdef GDM_DUMP_DEBUG
		void MatrixCSR::PrintCRS() const
		{
			std::ofstream myfile;
			myfile.open(debug_dump::DebugDump::path("test_Matrix.txt"));
			sendCRS2Stream(myfile);
			myfile.close();
		}

		void MatrixCSR::PrintDiagBlocks() const
		{
			std::ofstream myfile;
			myfile.open(debug_dump::DebugDump::path("test_diagValues.txt"));
			sendDiagVals2Stream(myfile);
			myfile.close();
		}
#endif

	} // linear_problem
} // reservoir_simulator
