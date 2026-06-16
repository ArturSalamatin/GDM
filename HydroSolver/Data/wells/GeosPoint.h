#pragma once
#include <memory>

namespace GeosShell {

class SimplePoint {
	double x_, y_;
public:
	SimplePoint() : x_(0), y_(0) {}
	SimplePoint(double x, double y) : x_(x), y_(y) {}
	double getX() const { return x_; }
	double getY() const { return y_; }
};

class GeosPoint {
public:
	GeosPoint() : pt_(std::make_shared<SimplePoint>()) {}
	GeosPoint(double x, double y) : pt_(std::make_shared<SimplePoint>(x, y)) {}

	const std::shared_ptr<SimplePoint>& get() const { return pt_; }
	std::shared_ptr<SimplePoint>& get() { return pt_; }

private:
	std::shared_ptr<SimplePoint> pt_;
};

} // GeosShell
