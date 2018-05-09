#include "Localization3D.h"
#include "Base.h"
#include "Config.h"

#include <cmath>
#include <iostream>
#include <set>
#include <algorithm>
#include <climits>
#include <chrono>

using namespace std;

// For stopping in time
auto g_stopping = chrono::system_clock::now();
bool g_fast_calculation = false;

bool g_use_2d = false;

// Needed by Point
static bool equal(double a, double b);

class Point {
public:
	explicit Point(const string& ip) {
		ip_ = ip;
		set_ = false;
	}
	
	explicit Point(const array<double, 3>& coordinates) {
		coordinates_ = coordinates;
		ip_ = "";
		set_ = true;
	}
	
	void setPosition(const array<double, 3>& coordinates) {
		coordinates_ = coordinates;
		set_ = true;
	}
	
	const array<double, 3>& getPosition() const {
		return coordinates_;
	}
	
	const string& getIP() {
		return ip_;
	}
	
	double getX() const {
		return coordinates_.front();
	}
	
	double getY() const {
		return coordinates_.at(1);
	}
	
	double getZ() const {
		return coordinates_.back();
	}
	
	void addDistance(double distance) {
		distances_.push_back(distance);	
	}
	
	double getDistance(size_t i) const {
		return distances_.at(i);
	}
	
	double distanceTo(const Point& point) {
		double sum = 0;
		
		for (size_t i = 0; i < coordinates_.size(); i++)
			sum += (coordinates_.at(i) - point.getPosition().at(i)) * (coordinates_.at(i) - point.getPosition().at(i));
			
		return sqrt(sum);	
	}
	
	bool operator==(const Point& other) {
		for (size_t i = 0; i < coordinates_.size(); i++)
			if (!equal(coordinates_.at(i), other.getPosition().at(i)))
				return false;
				
		return true;		
	}
	
	bool operator<(const Point& other) const {
		for (size_t i = 0; i < coordinates_.size(); i++)
			if (!equal(coordinates_.at(i), other.getPosition().at(i)))
				return abs(coordinates_.at(i)) < abs(other.getPosition().at(i));
				
		return false;		
	}
	
	friend ostream& operator<<(ostream& out, const Point& point) {
		cout << "(" << point.getX() << ", " << point.getY() << ", " << point.getZ() << ")";
		
		return out;
	}
	
private:
	vector<double> distances_;
	array<double, 3> coordinates_ = {{ 0, 0, 0 }};
	string ip_;
	bool set_;
};

static double PI;
static int g_degree_accuracy;
static double g_point_accuracy;

// 0.001 is enough precision for Localization3D
static bool equal(double a, double b) {
	return abs(a - b) < 0.001;
}

static double radians(double degrees) {
	return (degrees * PI) / 180;
}

static bool sortZ(const Point& a, const Point& b) {
	return abs(a.getZ()) < abs(b.getZ());
}

static vector<Point> getSinglePossibles(const Point& point, double actual_distance) {
	vector<Point> possibles;
	
	if (g_use_2d) {
		for (int gamma = 0; gamma < 360; gamma += g_degree_accuracy) {
			double x = point.getX() + actual_distance * cos(radians(gamma));
			double y = point.getY() + actual_distance * sin(radians(gamma));
				
			possibles.push_back(Point(array<double, 3>({{ x, y, 0.0 }})));
		}
	} else {
		for (int gamma = 0; gamma < 360; gamma += g_degree_accuracy) {
			for (int omega = 0; omega < 360; omega += g_degree_accuracy) {
				double x = point.getX() + actual_distance * cos(radians(gamma)) * sin(radians(omega));
				double y = point.getY() + actual_distance * sin(radians(gamma)) * sin(radians(omega));
				double z = point.getZ() + actual_distance * cos(omega);
				
				possibles.push_back(Point(array<double, 3>{{ x, y, z }}));
			}
		}
	}
	
	return possibles;
}

template<class T>
static void removeDuplicates(vector<T>& data) {
	set<T> singulars;
	size_t size = data.size();
	
	for (size_t i = 0; i < size; i++)
		singulars.insert(data[i]);
		
	data.assign(singulars.begin(), singulars.end());
}

static vector<Point> getPossibles(const vector<Point>& points, size_t i) {
	vector<Point> possibles;
	vector<Point> working;
	
	#pragma omp parallel
	{
		vector<Point> parallel_possibles;
		
		#pragma omp for
		for (size_t j = 0; j < points.size(); j++) {
			const Point& master = points.at(j);
			double distance = master.getDistance(i);
			auto master_possibles = getSinglePossibles(master, distance);
			
			parallel_possibles.insert(parallel_possibles.end(), master_possibles.begin(), master_possibles.end());
		}
		
		#pragma omp critical
		{
			possibles.insert(possibles.end(), parallel_possibles.begin(), parallel_possibles.end());
		}
	}
	
	removeDuplicates(possibles);
	
	#pragma omp parallel
	{
		vector<Point> parallel_working;
		
		#pragma omp for
		for (size_t j = 0; j < possibles.size(); j++) {
			Point& point = possibles.at(j);
			bool good = true;
			
			for (auto& origin : points) {
				double distance = origin.getDistance(i);
				double test_distance = point.distanceTo(origin);
				
				if (abs(distance - test_distance) > g_point_accuracy) {
					good = false;
					
					break;
				}
			}

			if (good) {
				parallel_working.push_back(point);
			}
		}
		
		#pragma omp critical
		{
			working.insert(working.end(), parallel_working.begin(), parallel_working.end());
		}
	}
	
	sort(working.begin(), working.end(), sortZ);
	
	return working;
}

static vector<Point> getPlacement(vector<Point> points, size_t start) {
	if (start >= points.size())
		return points;
		
	vector<Point> origins(points.begin(), points.begin() + start);
	vector<Point> possibles = getPossibles(origins, start);
	
	// Do we have any possibility?
	if (possibles.empty())
		return vector<Point>();
	
	// We do, let's see if this is the last point
	if (points.size() == start + 1) {
		points.at(start).setPosition(possibles.front().getPosition());
		
		return points;
	}
	
	// It's not the last point, let's try every possiblity
	volatile bool flag = false;
	vector<Point> final_result;
	
	#pragma omp parallel for shared(flag)
	for (size_t i = 0; i < possibles.size(); i++) {
		if (flag || ((chrono::system_clock::now() - g_stopping).count() > 0 && g_fast_calculation))
			continue;
			
		auto& possible = possibles.at(i);
		points.at(start).setPosition(possible.getPosition());
		
		vector<Point> result = getPlacement(points, start + 1);
		
		if (!result.empty()) {
			#pragma omp critical
			{
				final_result = result;
				flag = true;
			}
		}
	}
	
	return final_result;
}

double diffZ(const vector<Point>& points) {
	double sum = 0;
	
	for (auto& point : points)
		sum += (point.getZ() * point.getZ());
		
	return sqrt(sum);
}

vector<array<double, 3>> Localization3D::run(const Localization3DInput& input, bool fast_calcuation) {
	g_degree_accuracy = Base::config().get<int>("degree_accuracy");
	g_point_accuracy = Base::config().get<double>("point_accuracy");
	PI = atan(1) * 4;
	
	vector<Point> points;
	
	//cout << "Debug: input size " << input.size() << endl;
	
	for (auto& peer : input) {
		auto& ip = peer.first;
		auto& distances = peer.second;
		
		Point point(ip);
		
		for (auto& distance : distances) {
			point.addDistance(distance);
			
			//cout << "Debug: added distance " << distance << endl;
		}
			
		points.push_back(point);	
	}
	
	// Need to have some kind of reference
	points.front().setPosition({{ 0, 0, 0 }});
	
	double best_point = INT_MAX;
	double best_z_diff = INT_MAX;
	vector<Point> best_points;
	bool has_solution = false;
	
	//cout << "Debug: running localization\n";
	
	g_stopping = chrono::system_clock::now() + chrono::seconds(Base::config().get<int>("timeout"));
	g_fast_calculation = fast_calcuation;
	
	g_use_2d = Base::config().get<bool>("use_2d");
	
	bool stop = false;
	
	while (g_degree_accuracy > 0) {
		cout << "Debug: trying degree " << g_degree_accuracy << endl;
		
		g_point_accuracy = Base::config().get<double>("point_accuracy");

		while (g_point_accuracy > 0) {
			// Check time limit here
			if ((chrono::system_clock::now() - g_stopping).count() > 0 && fast_calcuation) {
				cout << "Debug: " << "Execution timed out\n";
				stop = true;
				
				break;
			}
				
			vector<Point> basic_points(points);
			auto results = getPlacement(basic_points, 1);
			
			if (results.empty())
				break;
			
			// If the current best result has the same point accuracy better z_diff, ignore it
			if (g_point_accuracy < best_point || (equal(g_point_accuracy, best_point) && diffZ(results) < best_z_diff)) {
				// New best result
				best_point = g_point_accuracy;
				best_z_diff = diffZ(results);
				best_points = results;
				has_solution = true;
			}
			
			g_point_accuracy -= 0.01;
		}
		
		if (stop && fast_calcuation)
			break;
		
		g_degree_accuracy--;
	}
	
	vector<array<double, 3>> final_result;
	
	if (has_solution) {
		cout << "Debug: Localization3D got solution with accuracy " << best_point << " m and z_diff " << best_z_diff << endl; 
		
		for (auto& point : best_points) {
			final_result.push_back(point.getPosition());
		}
		
		return final_result;
	} else {
		cout << "Debug: no solution available\n";
		
		for (auto& point : points) {
			final_result.push_back(point.getPosition());
		}
	}
	
	return final_result;
}