#pragma once

#include <sserialize/Static/Array.h>
#include <sserialize/utility/checks.h>

namespace srtree::detail {

template<typename T_BASE_TRAIT>
class DedupDeserializationTraitsAdapter: public T_BASE_TRAIT {
public:
	using Parent = T_BASE_TRAIT;
	using Signature = typename Parent::Signature;
public:
	class Deserializer {
	public:
		Deserializer(DedupDeserializationTraitsAdapter const * that) :
		m_that(that)
		{}
	public:
		Signature operator()(sserialize::UByteArrayAdapter const & d) {
			return m_that->signature( d.getUint32(0) );
		}
	private:
		DedupDeserializationTraitsAdapter const * m_that;
	};
public:
	DedupDeserializationTraitsAdapter() {}
	DedupDeserializationTraitsAdapter(sserialize::UByteArrayAdapter const & d) :
	Parent(d),
	m_d(d + sserialize::SerializationInfo<Parent>::sizeInBytes( static_cast<Parent const &>(*this) ) )
	{}
	DedupDeserializationTraitsAdapter(DedupDeserializationTraitsAdapter const &) = default;
	DedupDeserializationTraitsAdapter(DedupDeserializationTraitsAdapter && other) = default;
	~DedupDeserializationTraitsAdapter() override {}
	DedupDeserializationTraitsAdapter & operator=(DedupDeserializationTraitsAdapter const &) = default;
	DedupDeserializationTraitsAdapter & operator=(DedupDeserializationTraitsAdapter &&) = default;
	sserialize::UByteArrayAdapter::SizeType getSizeInBytes() const {
		return sserialize::SerializationInfo<Parent>::sizeInBytes( static_cast<Parent const &>(*this) ) 
			+ sserialize::SerializationInfo<decltype(m_d)>::sizeInBytes(m_d);
	}
public:
	Deserializer deserializer() const {
		return Deserializer(this);
	}
public:
	Signature signature(uint32_t id) const { return m_d.at(id); }
private:
	template<typename U>
	friend sserialize::UByteArrayAdapter & operator>>(sserialize::UByteArrayAdapter & src, DedupDeserializationTraitsAdapter<U> & dest);
private:
	sserialize::Static::Array<Signature> m_d;
};

template<typename U>
inline
sserialize::UByteArrayAdapter &
operator>>(sserialize::UByteArrayAdapter & src, DedupDeserializationTraitsAdapter<U> & dest) {
	sserialize::UByteArrayAdapter tmp(src);
	tmp.shrinkToGetPtr();
	dest = DedupDeserializationTraitsAdapter<U>(tmp);
	src.incGetPtr(sserialize::SerializationInfo< DedupDeserializationTraitsAdapter<U> >::sizeInBytes(dest) );
	return src;
}
	
}//end namespace
