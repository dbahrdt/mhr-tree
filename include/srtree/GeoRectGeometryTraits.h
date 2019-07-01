#pragma once

#include <sserialize/spatial/GeoRect.h>

namespace srtree::detail {
	
class GeoRectGeometryTraits {
public:
	using Boundary = sserialize::spatial::GeoRect;
	
	class MayHaveMatch {
	public:
		inline bool operator()(Boundary const & x) const { return m_ref.overlap(x); }
	private:
		friend class GeoRectGeometryTraits;
	private:
		MayHaveMatch(Boundary const & ref) : m_ref(ref) {}
	private:
		Boundary m_ref;
	};
public:
	MayHaveMatch mayHaveMatch(Boundary const & ref) const { return MayHaveMatch(ref); }
};
	
}//end namespace srtree::detail
