#include <srtree/GeoConstraint.h>

namespace srtree {

GeoConstraint::GeoConstraint(sserialize::spatial::GeoRect const & rect) :
m_d(1, rect)
{}

GeoConstraint::~GeoConstraint() {}

GeoConstraint GeoConstraint::operator+(GeoConstraint const & other) const {
	GeoConstraint result(*this);
	result += other;
	return result;
}

GeoConstraint &
GeoConstraint::operator+=(GeoConstraint const & other) {
	m_d.insert(m_d.end(), other.m_d.begin(), other.m_d.end());
	return *this;
}

GeoConstraint
GeoConstraint::operator/(GeoConstraint const & other) const {
	GeoConstraint result(*this);
	result /= other;
	return result;
}

GeoConstraint &
GeoConstraint::operator/=(GeoConstraint const & other) {
	std::vector<sserialize::spatial::GeoRect> result;
	for(auto const & x : m_d) {
		for(auto const & y : other.m_d) {
			if (x.overlap(y)) {
				result.push_back(x / y);
			}
		}
	}
	m_d = std::move(result);
	return *this;
}

bool
GeoConstraint::empty() const {
	return !m_d.size();
}

bool
GeoConstraint::intersects(sserialize::spatial::GeoRect const & other) const {
	for(auto const & x : m_d) {
		if (x.overlap(other)) {
			return true;
		}
	}
	return false;
}

}
