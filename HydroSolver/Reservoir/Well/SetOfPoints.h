#pragma once
#include "../../stdafx.h"
#include "../../Helpers/LogFile.h"


namespace set_of_points
{

	/// <summary>
	/// Class representing a sengment of a line
	/// </summary>
	struct Segment
	{
		double start, end; // segment coordinates
		double itsLength; // overall segment length

		Segment(const Segment&) = default;
		Segment(double s, double e) noexcept;
		Segment(const std::pair<double, double>& segm) noexcept;
		double length() const;
		std::pair<double, double> segment() const;

		bool operator== (const Segment& compareWith) const noexcept;
	};

	/// <summary>
	/// Class representing the type of work on the well: open(true) or close(false)
	/// </summary>
	struct WellJob
	{
		Segment segment; // segment coordinates

		WellJob(const Segment& s, bool f) noexcept;
		WellJob(const std::pair<double, double>& segm, bool f) noexcept;
		WellJob(double start, double end, bool f) noexcept;

		bool isOpen() const noexcept;
	protected:
		bool is_open;
	};

	/// <summary>
	/// Class representing well operation with time_stamp assigned
	/// </summary>
	struct WellJobTime :WellJob
	{
		double timeMoment;

		WellJobTime(const Segment& s, bool f, double t) noexcept;
		WellJobTime(const std::pair<double, double>& segm, bool f, double t) noexcept;
		WellJobTime(double start, double end, bool f, double t) noexcept;

		static bool sortOperator(const WellJobTime& a, const WellJobTime& b) noexcept;

		/// <summary>
		/// Some operations should be reordered within a MER-month
		/// </summary>
		/// <param name="newTime"></param>
		void TransferDate(double newTime);
	};

	class SetOfPoints
	{// https://ru.stackoverflow.com/questions/679634/%D0%9E%D0%B1%D1%8A%D0%B5%D0%B4%D0%B8%D0%BD%D0%B5%D0%BD%D0%B8%D0%B5-%D0%B8-%D1%80%D0%B0%D0%B7%D0%BD%D0%BE%D1%81%D1%82%D1%8C-%D0%BE%D1%82%D1%80%D0%B5%D0%B7%D0%BA%D0%BE%D0%B2
	// https://ru.stackoverflow.com/questions/671814/%D0%9F%D0%BE%D0%B8%D1%81%D0%BA-%D0%BE%D0%B1%D1%8A%D0%B5%D0%B4%D0%B8%D0%BD%D0%B5%D0%BD%D0%B8%D1%8F-%D0%BF%D1%80%D0%BE%D0%BC%D0%B5%D0%B6%D1%83%D1%82%D0%BA%D0%BE%D0%B2/671881#671881

		/// <summary>
		/// Class representing the geometricall ray
		/// </summary>
		struct Ray
		{
			double val; // its coordinate on the line
			int pointType; // +1 --- begin of the OPEN segment, -1 --- its end
			// -infty --- begin of the CLOSE segment, +infty --- its end

			Ray(double v, int pT) noexcept;

			void MakeRegular();

			static bool sortOperator(const Ray& a, const Ray& b);
		};

	public:

		std::vector<Ray> points;
		std::vector<Segment> itsSegments;

	public:

		SetOfPoints(const WellJob& wellJob) noexcept;

		void AddSegment(const WellJob& wellJob);

		void AddSegment(const std::pair<double, double>& s_e, bool f);

		void push_back(double start, double end);

		void Sweep();

		void CleanSegmentsPoints();

		const std::vector<Segment>& getCurrentConfiguration() const;

		double totalLength() const;

		bool operator==(const SetOfPoints& rhs) const;

	//	bool IsSame(const SetOfPoints& compareWith) const;

#pragma region debugFunctions
		void printSegments() const;
		void printPoints() const;
#pragma endregion
	};


	/// <summary>
	/// Class representing perforations based on performed well operations
	/// </summary>
	class SetOfPerforations
	{
	protected:
		double timeMoment;
		SetOfPoints itsPerforations; // starting from timeMoment

	public:
		SetOfPerforations(const WellJobTime& wellJob) noexcept;

		// default perforation is at initial -- closed state
		SetOfPerforations() noexcept;

		void AddNewJob(const WellJobTime& wellJob);
		void MoveDate(double newDate);
		SetOfPerforations JoinPerforations(const SetOfPerforations& P2);

		double curTime() const;
		const SetOfPoints& perforations() const;
		double TotalLength() const;
		bool operator==(const SetOfPerforations& rhs) const;
	};

	// accmulated perforations in a single cell, i.e., upscaled layer opened by the well
	class AccumulatedPerforations
	{
	protected:
		//	double ItsEarliestTime;
		std::vector<double> ItsAllJobsMoments;
		std::vector<SetOfPerforations> RawPerorationsInTime; // raw std::vector of current sets of perforations
		std::vector<SetOfPerforations> PerforationsInTime; // averaged out std::vector of current sets of perforations
		
	public:
		AccumulatedPerforations() noexcept;
		AccumulatedPerforations(const WellJobTime& wellJob) noexcept;

		double EarliestJobDate() const;
		double LatestJobDate() const;
		const std::vector<double>& AllJobMoments() const;
		double NextJobMoment(double timeMoment) const;

		void AddNewJob(const WellJobTime& wellJob);

		const std::vector<SetOfPerforations>& getPerforationsSet() const;
		std::vector<SetOfPerforations>& getPerforationsSet();
		const SetOfPerforations& getPerforations(double time) const;

		const SetOfPerforations& getPerforations_future(double time) const;
		void AveragePerforationsOut(
			const std::vector<std::pair<size_t, std::pair<double, double>>>& jobIntervals);

		void AssembleJobDates();
		void ReverseAveraging();

		void RemoveRedundantJobs();
	};

	/// <summary>
	/// Map sets a relation between the layer and corresponding history of accumulated perforations  
	/// </summary>
	using PerforationsOfWell = std::map<size_t, AccumulatedPerforations>;


	


} // set_of_points