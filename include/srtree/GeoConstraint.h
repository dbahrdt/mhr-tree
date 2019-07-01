#pragma once

#include <sserialize/spatial/GeoRect.h>

namespace srtree {
	
class GeoConstraint {
public:
	GeoConstraint(sserialize::spatial::GeoRect const & rect);
	GeoConstraint(GeoConstraint const & other);
	GeoConstraint(GeoConstraint && other);
	~GeoConstraint();
public:
	GeoConstraint operator+(GeoConstraint const & other) const;
	GeoConstraint & operator+=(GeoConstraint const & other);
	GeoConstraint operator/(GeoConstraint const & other) const;
	GeoConstraint & operator/=(GeoConstraint const & other);
public:
	bool empty() const;
	bool intersects(sserialize::spatial::GeoRect const & other) const;
private:
	std::vector<sserialize::spatial::GeoRect> m_d;
};
	
}//end namespace srtree
