#pragma once

#include <srtree/GeoConstraint.h>

namespace srtree::detail {
	
class GeoRectGeometryTraits final {
public:
	using Boundary = sserialize::spatial::GeoRect;
	
	class MayHaveMatch {
	public:
		inline bool operator()(Boundary const & x) const { return m_ref.intersects(x); }
		inline MayHaveMatch operator+(MayHaveMatch const & other) const { return MayHaveMatch(m_ref + other.m_ref); }
		inline MayHaveMatch operator/(MayHaveMatch const & other) const { return MayHaveMatch(m_ref / other.m_ref); }
	private:
		friend class GeoRectGeometryTraits;
	private:
		MayHaveMatch(Boundary const & ref) : m_ref(ref) {}
		MayHaveMatch(GeoConstraint const & gc) : m_ref(gc) {}
	private:
		GeoConstraint m_ref;
	};
	
	class Serializer {
	public:
		inline sserialize::UByteArrayAdapter & operator()(sserialize::UByteArrayAdapter & dest, Boundary const & b) const {
			return dest << b;
		}
	};
	
	class Deserializer {
	public:
		inline std::size_t operator()(sserialize::UByteArrayAdapter const & dest, Boundary & b) const {
			b = Boundary(dest);
			return sserialize::SerializationInfo<Boundary>::sizeInBytes(b);
		}
	};
	
public:
	GeoRectGeometryTraits() {}
	GeoRectGeometryTraits(sserialize::UByteArrayAdapter const & d) {
		SSERIALIZE_VERSION_MISSMATCH_CHECK(1, d.at(0), "GeoRectGeometryTraits");
	}
	~GeoRectGeometryTraits() {}
	sserialize::UByteArrayAdapter::SizeType getSizeInBytes() const {
		return sserialize::SerializationInfo<uint8_t>::length;
	}
	MayHaveMatch mayHaveMatch(Boundary const & ref) const { return MayHaveMatch(ref); }
	Serializer serializer() const { return Serializer(); }
	Deserializer deserializer() const { return Deserializer(); }
};

inline sserialize::UByteArrayAdapter & operator<<(sserialize::UByteArrayAdapter & dest, srtree::detail::GeoRectGeometryTraits const &) {
	dest << uint8_t(1);
	return dest;
}

}//end namespace srtree::detail


