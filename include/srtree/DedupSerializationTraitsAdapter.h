#pragma once

#include <sserialize/containers/VariantStore.h>
#include <sserialize/utility/checks.h>

namespace srtree::detail {

template<typename T_BASE_TRAIT>
class DedupSerializationTraitsAdapter: public T_BASE_TRAIT {
public:
	using Parent = T_BASE_TRAIT;
	using Signature = typename Parent::Signature;
public:
	class Serializer {
	public:
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
			return dest << sserialize::narrow_check<uint32_t>( m_vs->insert(m_cache) );
		}
	private:
		sserialize::UByteArrayAdapter m_cache{sserialize::MM_PROGRAM_MEMORY};
		typename Parent::Serializer m_base;
		std::shared_ptr<sserialize::VariantStore> m_vs;
	};
private:
	//deserialization needs another trait
	class Deserializer;
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
	return dest << static_cast<typename DedupSerializationTraitsAdapter<U>::Parent &>(v) << v.m_vs->getFlushedData();
}
	
}//end namespace
