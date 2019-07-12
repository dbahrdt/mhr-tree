#pragma once

#include <sserialize/Static/Array.h>
#include <sserialize/utility/checks.h>
#include <sserialize/Static/Version.h>

namespace srtree::detail {

template<typename T_BASE_TRAIT>
class DedupDeserializationTraitsAdapter: private sserialize::Static::SimpleVersion<1>,  public T_BASE_TRAIT {
public:
	using Version = sserialize::Static::SimpleVersion<1>;
	using Parent = T_BASE_TRAIT;
	using Signature = typename Parent::Signature;
public:
	class Deserializer {
	public:
		using Type = uint32_t;
	public:
		Deserializer(DedupDeserializationTraitsAdapter const * that) :
		m_that(that)
		{}
	public:
		Signature operator()(uint32_t v) {
			return m_that->signature(v);
		}
	private:
		DedupDeserializationTraitsAdapter const * m_that;
	};
public:
	DedupDeserializationTraitsAdapter() {}
	DedupDeserializationTraitsAdapter(sserialize::UByteArrayAdapter d) :
	Version(d, Version::Consume()),
	Parent(d)
	{
		d += sserialize::SerializationInfo<Parent>::sizeInBytes( static_cast<Parent const &>(*this) );
		d >> m_d;
	}
	DedupDeserializationTraitsAdapter(DedupDeserializationTraitsAdapter const &) = default;
	DedupDeserializationTraitsAdapter(DedupDeserializationTraitsAdapter && other) = default;
	~DedupDeserializationTraitsAdapter() override {}
	DedupDeserializationTraitsAdapter & operator=(DedupDeserializationTraitsAdapter const &) = default;
	DedupDeserializationTraitsAdapter & operator=(DedupDeserializationTraitsAdapter &&) = default;
	sserialize::UByteArrayAdapter::SizeType getSizeInBytes() const {
		return 1+sserialize::SerializationInfo<Parent>::sizeInBytes( static_cast<Parent const &>(*this) ) 
			+ sserialize::SerializationInfo<decltype(m_d)>::sizeInBytes(m_d);
	}
public:
	Deserializer deserializer() const {
		return Deserializer(this);
	}
public:
	Signature signature(uint32_t id) const { return Signature( m_d.at(id) ); }
private:
	sserialize::Static::Array<sserialize::UByteArrayAdapter> m_d;
};

	
}//end namespace
