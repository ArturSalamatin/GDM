#include "SetOfPoints.h"
#include <cassert>
#include <iostream>
#include "../../Data/ExceptionFactory.h"

namespace set_of_points
{
	std::ostream& operator<<(std::ostream& o, const Segment& s)
	{
		o
			<< "("
			<< s.start
			<< "; "
			<< s.end
			<< ") length = "
			<< s.length()
			<< std::endl;
		return o;
	}

	template<typename O>
	O& operator<<(O& o, const SetOfPoints::Ray& r)
	{
		o
			<< r.val
			<< ", "
			<< r.pointType;

			return o;
	}

	Segment::Segment(double s, double e) noexcept :
		start{ std::min(s,e)},
		end{ std::max(s,e)},
		itsLength{ std::max(s,e) - std::min(s,e) }
	{
	//	assert(itsLength <= 0.0);
		if (itsLength < 0.0)
			std::cout << *this;
	}

	Segment::Segment(const std::pair<double, double>& segm) noexcept :
		Segment{ segm.first, segm.second }
	{}

	double Segment::length() const
	{
		return itsLength;
	}

	std::pair<double, double> Segment::segment() const
	{
		return std::make_pair(start, end);
	}

	bool Segment::operator== (const Segment& rhs) const noexcept
	{
		return (start == rhs.start) && (end == rhs.end);
	}




	WellJob::WellJob(const Segment& s, bool f) noexcept :
		segment{ s }, is_open{ f }
	{}

	WellJob::WellJob(const std::pair<double, double>& segm, bool f) noexcept :
		WellJob{ Segment{segm}, f }
	{}

	WellJob::WellJob(double start, double end, bool f) noexcept :
		WellJob{ Segment{start, end}, f }
	{}

	bool WellJob::isOpen() const noexcept
	{
		return is_open;
	}






	WellJobTime::WellJobTime(const Segment& s, bool f, double t) noexcept :
		WellJob{ s, f },
		timeMoment{ t }
	{}

	WellJobTime::WellJobTime(const std::pair<double, double>& segm, bool f, double t) noexcept :
		WellJobTime{ Segment{segm}, f, t }
	{}

	WellJobTime::WellJobTime(const double start, double end, bool f, double t) noexcept :
		WellJobTime(Segment{ start, end }, f, t)
	{}

	bool WellJobTime::sortOperator(
		const WellJobTime& a, 
		const WellJobTime& b)  noexcept
	{
		return (a.timeMoment < b.timeMoment) || (a.timeMoment == b.timeMoment) && (a.isOpen() < b.isOpen());
	}

	void WellJobTime::TransferDate(double newTime)
	{
		assert(newTime <= timeMoment);
		if (newTime > timeMoment)
			reservoir_simulator::ExceptionFactory::WellJobTime_TransferDate();
		timeMoment = newTime;
	}





	SetOfPoints::Ray::Ray(double v, int pT) noexcept :
		val{ v },
		pointType{ pT }
	{}

	void SetOfPoints::Ray::MakeRegular()
	{
		if (pointType > 0) pointType = 1;
		else pointType = -1;
	}

	bool SetOfPoints::Ray::sortOperator(const Ray& a, const Ray& b)
	{
		return (a.val < b.val) || (a.val == b.val) && (a.pointType > b.pointType);
	}





	SetOfPoints::SetOfPoints(const WellJob& wellJob) noexcept
	{
		AddSegment(wellJob);
	}

	void SetOfPoints::AddSegment(const WellJob& wellJob)
	{
		AddSegment(wellJob.segment.segment(), wellJob.isOpen());
	}

	void SetOfPoints::AddSegment(const std::pair<double, double>& s_e, bool f)
	{// f --- true for opening job, false --- for the closing job
		int delta;
		if (f) delta = 1;
		else delta = -2147483647 / 100;

		// add end-points to the collection of points
		points.emplace_back(s_e.first, delta);
		points.emplace_back(s_e.second, -delta);
		// sort the collection of points
		std::sort(points.begin(), points.end(), Ray::sortOperator);

		//	printPoints();

		// get the union, overlapping... of all segments
		Sweep(); // updates the itsSegments std::vector
		CleanSegmentsPoints();
	}

	void SetOfPoints::push_back(double start, double end)
	{
		itsSegments.emplace_back(start, end);
	}

	void SetOfPoints::Sweep()
	{
		itsSegments.clear();

		// accumulate pointTypes (or deltas from AddSegment)
		std::vector<int> sum;

		sum.push_back(points[0].pointType);
		for (size_t i = 1; i < points.size(); ++i)
			sum.push_back(sum.back() + points[i].pointType);

		size_t i = 0;
		while (i < sum.size())
		{
			if (((i + 1) % 2) && sum[i] <= 0)
			{
				points.erase(points.begin() + i);
				sum.erase(sum.begin() + i);
				continue;
			}
			if (((i + 1) % 2) && sum[i] > 0)
			{
				i++;
				continue;
			}
			if (((i) % 2) && sum[i] > 0)
			{
				points.erase(points.begin() + i);
				sum.erase(sum.begin() + i);
				continue;
			}
			if (((i) % 2) && sum[i] <= 0)
			{
				this->push_back(points[i - 1].val, points[i].val);
				i++;
				continue;
			}
		}

		points.shrink_to_fit();
		for (size_t i = 0; i < points.size(); ++i)
			points[i].MakeRegular();

	}

	void SetOfPoints::CleanSegmentsPoints()
	{
		for (size_t i = itsSegments.size(); i > 0; --i)
		{
			if (itsSegments[i - 1].length() == 0.0)
			{
				itsSegments.erase(itsSegments.begin() + i - 1);
				points.erase(points.begin() + 2 * (i - 1));
				points.erase(points.begin() + 2 * (i - 1));
			}
		}
	}

	const std::vector<Segment>& SetOfPoints::getCurrentConfiguration() const
	{
		return itsSegments;
	}

	double SetOfPoints::totalLength() const
	{
		double sum = 0;
		//		if (!itsSegments.empty())
		for (const auto& segm : itsSegments)
			sum += segm.length();
		return sum;
	}

	bool SetOfPoints::operator==(const SetOfPoints& rhs) const
	{
		bool f = rhs.itsSegments.size() == itsSegments.size();
		if (!f) return false;
		for (size_t i = 0; i < itsSegments.size(); ++i)
			if (!(itsSegments[i] == rhs.itsSegments[i])) {}
		return true;
	}

	void SetOfPoints::printSegments() const
	{
		if (itsSegments.size() == 0)
			std::cout << "There are no segments yet!!" << std::endl;
		else
		{
			std::cout << "The obtained segments are:" << '\n';
			for (int i = 0; i < itsSegments.size(); i++)
				std::cout << itsSegments[i];
			std::cout << std::endl;
		}
	}

	void SetOfPoints::printPoints() const
	{
		if (points.size() == 0)
			std::cout << "There are no points yet!!" << std::endl;
		else
		{
			std::cout << "The stored points are:" << '\n';
			for (size_t i = 0; i < points.size(); i += 2)
			{
				std::cout
					<< "("
					<< points[i]
					<< "; ";
				std::cout
					<< points[i + 1]
					<< ")" << '\n';
			}
			std::cout << std::endl;
		}
	}





	SetOfPerforations::SetOfPerforations(const WellJobTime& wellJob) noexcept :
		itsPerforations{ wellJob },
		timeMoment{wellJob.timeMoment}
	{}

	SetOfPerforations::SetOfPerforations() noexcept :
		SetOfPerforations{ WellJobTime{
		-std::numeric_limits<double>::max(), // segment start
		std::numeric_limits<double>::max(), // segment end
		false, // closed
		-std::numeric_limits<double>::max()} } // time stamp
	{}

	void SetOfPerforations::AddNewJob(const WellJobTime& wellJob)
	{
		timeMoment = wellJob.timeMoment;
		itsPerforations.AddSegment(wellJob);
	}

	double SetOfPerforations::curTime() const
	{
		return timeMoment;
	}

	const SetOfPoints& SetOfPerforations::perforations() const
	{
		return itsPerforations;
	}

	double SetOfPerforations::TotalLength() const
	{
		return perforations().totalLength();
	}

	bool SetOfPerforations::operator==(const SetOfPerforations& rhs) const
	{
		return itsPerforations == rhs.itsPerforations;
	}

	void SetOfPerforations::MoveDate(double newDate)
	{
		timeMoment = newDate;
	}

	SetOfPerforations 
		SetOfPerforations::JoinPerforations(const SetOfPerforations& P2)
	{
		SetOfPerforations P1{*this};
		for (const auto& p : P2.itsPerforations.itsSegments)
			P1.AddNewJob(
				WellJobTime{ 
					p, true, std::min(P2.curTime(), P1.curTime()) 
				});
		return P1;
	}




	double AccumulatedPerforations::EarliestJobDate() const 
	{ 
		// front() returns the imaginary job date, which is MIN_DOUBLE
	//	return ItsAllJobsMoments.front(); 
		// it is a real job date, which may not exist...
		assert(ItsAllJobsMoments.size() > 1);
		return ItsAllJobsMoments[1];
	}

	double AccumulatedPerforations::LatestJobDate() const 
	{ return ItsAllJobsMoments.back(); }

	const std::vector<double>& 
		AccumulatedPerforations::AllJobMoments() const 
	{ return ItsAllJobsMoments; }

	double AccumulatedPerforations::NextJobMoment(double timeMoment) const
	{
		for (size_t i = 0; i < AllJobMoments().size(); ++i)
			if (timeMoment < AllJobMoments()[i])
				return AllJobMoments()[i];

		return std::numeric_limits<double>::max();
	}


	AccumulatedPerforations::AccumulatedPerforations() noexcept :
		PerforationsInTime(1, SetOfPerforations{})
	{
	}


	AccumulatedPerforations::AccumulatedPerforations(
		const WellJobTime& wellJob) noexcept :
		AccumulatedPerforations{}
	{
		PerforationsInTime.emplace_back(wellJob);
	}

	void AccumulatedPerforations::AddNewJob(const WellJobTime& wellJob)
	{
		// perforations must be added one after the other in time
		if (wellJob.timeMoment < PerforationsInTime.back().curTime())
			reservoir_simulator::ExceptionFactory::AccumulatedPerforations_AddNewJob();

		if (wellJob.timeMoment > PerforationsInTime.back().curTime())
		{// initialize new set of jobs perfomed at the new date
			PerforationsInTime.push_back(PerforationsInTime.back()); // make a copy of a previous one
		}

		PerforationsInTime.back().AddNewJob(wellJob); // add to the copy a new segment
	}

	const std::vector<SetOfPerforations>& 
		AccumulatedPerforations::getPerforationsSet() const
	{
		return PerforationsInTime;
	}

	const SetOfPerforations& 
		AccumulatedPerforations::getPerforations(double time) const
	{
	//	if (getPerforationsSet()[0].curTime() > time)
	//		// the time is too early; no perforations yet
	//		return  SetOfPerforations{}; // default perforation state

		// assume that i = 0 perforationSet is the result
		// break the loop when a greater timeMoment is found

		// try to find the perforationsSet at later timeMoment
		for (size_t i = 1; i < getPerforationsSet().size(); ++i)
		{
			if (getPerforationsSet()[i].curTime() > time)
				return getPerforationsSet()[i - 1];
		}

		return getPerforationsSet().back(); // all perforations were made before the time-value, return the final configuration
	}

	const SetOfPerforations& 
		AccumulatedPerforations::getPerforations_future(double time) const
	{
		// try to find the perforationsSet at later timeMoment
		for (size_t i = 1; i < getPerforationsSet().size(); ++i)
			if (getPerforationsSet()[i].curTime() > time)
				return getPerforationsSet()[i];
		return getPerforationsSet().back(); // all perforations were made before the time-value, return the final configuration
	}

	void AccumulatedPerforations::AveragePerforationsOut(
		const std::vector<std::pair<size_t, std::pair<double, double>>>& jobIntervals)
	{
		// make a copy of accumulated data
		if (RawPerorationsInTime.empty())
			RawPerorationsInTime = PerforationsInTime;
		// bring all dates to the begin of MER frame
		for(const auto& [perf_id, interval]: jobIntervals)
	//	for (size_t i = 0; i < jobIntervals.size(); ++i)
			PerforationsInTime[perf_id].MoveDate(interval.first);
		// bring operations that close the entire cell to the next time frame
		for (size_t i = 1; i < jobIntervals.size(); ++i)
			if (PerforationsInTime[i-1].TotalLength() == 0.0) // the well becomes closed
				if (PerforationsInTime[i-1].curTime() < PerforationsInTime[i].curTime())
					PerforationsInTime[i-1].MoveDate(jobIntervals[i-1].second.second);
		if (PerforationsInTime.back().TotalLength() == 0.0)
			PerforationsInTime.back().MoveDate(jobIntervals.back().second.second);

		// merge all jobs from the same MER frame
		size_t i = 0;
		// size_t curDateFrame = 0;
		while (i < PerforationsInTime.size() - 1)
		{
		//	curDateFrame++;
			if (PerforationsInTime[i].curTime() == PerforationsInTime[i + 1].curTime())
			{
				auto p = PerforationsInTime[i].JoinPerforations(PerforationsInTime[i + 1]);
				PerforationsInTime[i] = p;
				PerforationsInTime.erase(PerforationsInTime.begin() + i + 1);
				continue;
			}
			i++;
		}
		//if (PerforationsInTime.back().TotalLength() == 0.0)
		//{// all perforations were closed in this moment. Transfer it to another date
		//	PerforationsInTime.back().MoveDate(jobIntervals[curDateFrame - 1].second);
		//}
		AssembleJobDates();
	}

	void AccumulatedPerforations::AssembleJobDates()
	{
		ItsAllJobsMoments.clear();
		for (const auto& perf : PerforationsInTime)
			ItsAllJobsMoments.push_back(perf.curTime());

	}

	void AccumulatedPerforations::ReverseAveraging()
	{
		// reverse the previous averaging procedure
		PerforationsInTime = RawPerorationsInTime;

		AssembleJobDates();
	}

	void AccumulatedPerforations::RemoveRedundantJobs()
	{
		size_t i = 0;
		while (i < PerforationsInTime.size() - 1)
		{
			// if states are the same
			if (PerforationsInTime[i] == PerforationsInTime[i + 1])
			{
				PerforationsInTime.erase(PerforationsInTime.begin() + i + 1);
				continue;
			}
			// if distance between states is negligable
			AssembleJobDates();
			/*		if (ItsAllJobsMoments[i + 1] - ItsAllJobsMoments[i] < 5.5)
			{
			if (PerforationsInTime[i].TotalLength() == 0)
			{
			PerforationsInTime.erase(PerforationsInTime.begin() + i);
			i--;
			continue;
			}
			}*/
			i++;
		}
	}


} // set_of_points