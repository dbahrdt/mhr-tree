#pragma once

#include <sserialize/containers/VariantStore.h>
#include <sserialize/utility/checks.h>
#include <srtree/Static/DedupDeserializationTraitsAdapter.h>

namespace srtree::detail {

template<typename T_BASE_TRAIT>
class DedupSerializationTraitsAdapter: public T_BASE_TRAIT {
public:
	using Parent = T_BASE_TRAIT;
	using Signature = typename Parent::Signature;
	using StaticTraits = srtree::detail::DedupDeserializationTraitsAdapter<typename T_BASE_TRAIT::StaticTraits>;
public:
	class Serializer {
	public:
		using Type = uint32_t;
		using BaseSerializer = typename Parent::Serializer;
	public:
		Serializer(std::shared_ptr<sserialize::VariantStore> const & vs, BaseSerializer && base) :
		m_base(std::forward<BaseSerializer>(base)),
		m_vs(vs)
		{}
		Serializer(Serializer const & ) = default;
	public:
		inline sserialize::UByteArrayAdapter & operator()(sserialize::UByteArrayAdapter & dest, Signature const & v) {
			m_cache.resize(0);
			m_base(m_cache, v);
			dest.put<uint32_t>( sserialize::narrow_check<uint32_t>( m_vs->insert(m_cache) ) );
			return dest;
		}
	private:
		sserialize::UByteArrayAdapter m_cache{sserialize::MM_PROGRAM_MEMORY};
		typename Parent::Serializer m_base;
		std::shared_ptr<sserialize::VariantStore> m_vs;
	};
	class Deserializer {
	public:
		using Type = uint32_t;
	public:
		Deserializer(std::shared_ptr<sserialize::VariantStore> const & vs) : m_vs(vs) {}
	public:
		Signature operator()(Type v) const {
			return Signature( m_vs->at(v) );
		}
	private:
		std::shared_ptr<sserialize::VariantStore> m_vs;
	};
public:
	template<typename... T_ARGS>
	DedupSerializationTraitsAdapter(T_ARGS... args) :
	Parent(std::forward<T_ARGS>(args)...),
	m_vs(std::make_shared<sserialize::VariantStore>(sserialize::MM_FAST_FILEBASED))
	{}
	DedupSerializationTraitsAdapter(Parent const & p) :
	Parent(p),
	m_vs(std::make_shared<sserialize::VariantStore>(sserialize::MM_FAST_FILEBASED))
	{}
	DedupSerializationTraitsAdapter(DedupSerializationTraitsAdapter const &) = default;
	DedupSerializationTraitsAdapter(DedupSerializationTraitsAdapter && other) = default;
	~DedupSerializationTraitsAdapter() override {}
	DedupSerializationTraitsAdapter & operator=(DedupSerializationTraitsAdapter const &) = default;
	DedupSerializationTraitsAdapter & operator=(DedupSerializationTraitsAdapter &&) = default;
public:
	inline Serializer serializer() const { return Serializer(m_vs, Parent::serializer()); }
private:
	template<typename U>
	friend sserialize::UByteArrayAdapter & operator<<(sserialize::UByteArrayAdapter & dest, DedupSerializationTraitsAdapter<U> & v);
private:
	std::shared_ptr<sserialize::VariantStore> m_vs;
};

template<typename U>
inline
sserialize::UByteArrayAdapter &
operator<<(sserialize::UByteArrayAdapter & dest, DedupSerializationTraitsAdapter<U> & v) {
	v.m_vs->flush();
	return dest << uint8_t(1) << static_cast<typename DedupSerializationTraitsAdapter<U>::Parent &>(v) << v.m_vs->getFlushedData();
}
	
}//end namespace
