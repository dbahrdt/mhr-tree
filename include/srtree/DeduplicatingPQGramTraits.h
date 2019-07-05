#pragma once

#include <srtree/PQGramTraits.h>
#include <sserialize/containers/VariantStore.h>

namespace srtree::detail {
	
class DeduplicatingPQGramTraits: public PQGramTraits {
public:
	class Serializer {
	public:
		Serializer(std::shared_ptr<sserialize::VariantStore> const & vs) : m_vs(vs) {}
		Serializer(Serializer const & ) = default;
	public:
		inline sserialize::UByteArrayAdapter & operator()(sserialize::UByteArrayAdapter & dest, Signature const & v) {
			m_cache.resize(0);
			m_base(m_cache, v);
			return dest << m_vs->insert(m_cache);
		}
	private:
		sserialize::UByteArrayAdapter m_cache{sserialize::MM_PROGRAM_MEMORY};
		PQGramTraits::Serializer m_base;
		std::shared_ptr<sserialize::VariantStore> m_vs;
	};
private:
	//deserialization needs another trait
	class Deserializer;
public:
	DeduplicatingPQGramTraits(uint32_t q = 3) : PQGramTraits(q), m_vs(std::make_shared<sserialize::VariantStore>(sserialize::MM_FAST_FILEBASED)) {}
	DeduplicatingPQGramTraits(DeduplicatingPQGramTraits const &) = default;
	DeduplicatingPQGramTraits(DeduplicatingPQGramTraits && other) = default;
	~DeduplicatingPQGramTraits() override {}
public:
	inline Serializer serializer() const { return Serializer(m_vs); }
private:
	friend sserialize::UByteArrayAdapter & operator<<(sserialize::UByteArrayAdapter & dest, DeduplicatingPQGramTraits & v);
private:
	std::shared_ptr<sserialize::VariantStore> m_vs;
};

inline
sserialize::UByteArrayAdapter &
operator<<(sserialize::UByteArrayAdapter & dest, DeduplicatingPQGramTraits & v) {
	v.m_vs->flush();
	return dest << v.db() << v.m_vs->getFlushedData();
}

}//end namespace srtree::detail
