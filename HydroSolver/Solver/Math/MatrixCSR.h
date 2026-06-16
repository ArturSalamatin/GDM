#pragma once
#include <memory>
#include <vector>

namespace reservoir_simulator
{
	namespace linear_problem
	{

		template<typename stream, typename T>
		void printValue(stream& o, T val)
		{
			o << std::setprecision(9) << std::scientific << std::setw(17) << val;
		}
		template<typename stream>
		void printZero(stream& o)
		{
			printValue(o, 0.0);
		}

		class SparsityPattern;

		class MatrixCSR
		{
		protected:
			std::unique_ptr<SparsityPattern> pattern;
			std::vector<double> value;

			size_t nnz; // number of non-zero elements in the matrix
			void CopyBlock(
				size_t valueOffset, std::vector<double>& dest, 
				const std::vector<double>& data, 
				const std::vector<size_t>& blockPosInValArray);

			unsigned char EqNmbr() const;
		public:

			size_t NmbrOfNonZerosPerUnitBlock() const;
			const std::vector<size_t>& Row()  const;
			const std::vector<size_t>& Col()  const;
			const std::vector<double>& Val()  const;

			void AddDiagBlock(size_t l, const std::vector<double>& data);
			void AddOffDiagBlock(size_t l, size_t neibIdx, std::vector<double>& data);

			MatrixCSR() noexcept;
			MatrixCSR(const size_t eqNmbr_, const size_t cellNmbr,
				const std::vector<std::vector<int>>& connectivityGraph) noexcept;

			MatrixCSR(size_t eqNmbr_, size_t cellNmbr,
				const std::vector<std::vector<int>>& connectivityGraph,
				const std::vector<bool>& blPattern) noexcept;

			virtual ~MatrixCSR();

			void ResetMatrix();

			void PrintCRS() const;
			void PrintDiagBlocks() const;

		private:

			const SparsityPattern& sparsity_pattern() const;

			template<typename stream>
			void sendCRS2Stream(stream& s, bool is_full = false) const
			{
				size_t i, j, k, zeroIndex;
				size_t mSize = Row().size() - 1;

				s << "Nonzero entries:" << std::endl;
				for (size_t i = 0; i < Col().size(); ++i)
					s << "(" << std::scientific << value[i] << "," << Col()[i] << ") ";
				s << std::endl << std::endl;
				s << "Outer pointers:" << std::endl;

				for (size_t i = 0; i < Row().size(); ++i)
					s << Row()[i] << " ";
				s << std::endl << std::endl;

				s << "Print matrix in CRS format as a " << mSize << "x" << mSize << " matrix" << std::endl;

				//Scan the row_ptr array to find the beginning and end of row i
				for (i = 0; i < mSize; ++i)
				{
					//the column index of the element to print
					zeroIndex = 0;

					// Print row i: index k goes from the beginning to the end of row i
					for (k = Row()[i]; k < Row()[i + 1]; ++k) {
						j = Col()[k];

						// print entries of zero values that exist between consecutive
						// non-zeroes
						while (zeroIndex < j) {
							if (is_full)
								printZero(s);
							++zeroIndex;
						} // while

						//print the nonzero value
						printValue(s, value[k]);
						// Prepare zeroIndex for the next sequence of zeroes
						++zeroIndex;
					} // for

					  //print the trailing zeroes in this row
					while (zeroIndex < mSize) {
						if (is_full)
							printZero(s);
						++zeroIndex;
					} // while

					// Start the next line
					s << std::endl;
				} // for

				s << std::endl;
			}

			template<typename stream>
			void sendDiagVals2Stream(stream& s) const
			{
				for (size_t l = 0; l < (Row().size() - 1) / EqNmbr(); ++l)
					for (size_t i = 0; i < EqNmbr(); ++i)
					{
						for (size_t j = 0; j < EqNmbr(); ++j)
						{
							printValue(s, value[sparsity_pattern().DiagBlocks()[EqNmbr() * (l * EqNmbr() + i) + j]]);
							s << "  ";
						}
						s << std::endl;
					}
			}
		};


	} // linear_problem
} // reservoir_simulator