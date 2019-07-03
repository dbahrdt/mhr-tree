#pragma once

#include <sserialize/spatial/GeoRect.h>

namespace srtree::detail {
	
class GeoRectGeometryTraits final {
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
	
	class Serializer {
		inline sserialize::UByteArrayAdapter & operator()(sserialize::UByteArrayAdapter & dest, Boundary const & b) const {
			return dest << b;
		}
	};
	
	class Deserializer {
		inline std::size_t operator()(sserialize::UByteArrayAdapter const & dest, Boundary & b) const {
			b = Boundary(dest);
			return sserialize::SerializationInfo<Boundary>::sizeInBytes(b);
		}
	};
	
public:
	GeoRectGeometryTraits() {}
	GeoRectGeometryTraits(sserialize::UByteArrayAdapter const &) {}
	~GeoRectGeometryTraits() {}
	MayHaveMatch mayHaveMatch(Boundary const & ref) const { return MayHaveMatch(ref); }
	Serializer serializer() const { return Serializer(); }
	Deserializer deserializer() const { return Deserializer(); }
};

inline sserialize::UByteArrayAdapter & operator<<(sserialize::UByteArrayAdapter & dest, srtree::detail::GeoRectGeometryTraits const &) {
	return dest;
}

inline sserialize::UByteArrayAdapter & operator>>(sserialize::UByteArrayAdapter & dest, srtree::detail::GeoRectGeometryTraits &) {
	return dest;
}

}//end namespace srtree::detail

