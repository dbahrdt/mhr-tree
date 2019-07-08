#pragma once

#include <srtree/MinWiseSignature.h>
#include <srtree/QGram.h>

#include <boost/rational.hpp>

#include <sserialize/algorithm/utilfunctional.h>
#include <sserialize/storage/UByteArrayAdapter.h>


namespace srtree::detail {

template<std::size_t T_SIZE, typename T_PARAMETRISED_HASH_FUNCTION>
class MinWiseSignatureTraits {
public:
	static constexpr std::size_t SignatureSize = T_SIZE;
	using HashFunction = T_PARAMETRISED_HASH_FUNCTION;
	using Signature = MinWiseSignature<SignatureSize, HashFunction::entry_bits>;
	using SignatureGenerator = MinWiseSignatureGenerator<SignatureSize, HashFunction::entry_bits, HashFunction>;
	
	class Serializer {
	public:
		inline sserialize::UByteArrayAdapter & operator()(sserialize::UByteArrayAdapter & dest, Signature const & sig) const {
			return dest << sig;
		}
	};
	
	class Deserializer {
	public:
		inline std::size_t operator()(sserialize::UByteArrayAdapter const & dest, Signature & sig) const {
			sig = Signature(dest);
			return sserialize::SerializationInfo<Signature>::sizeInBytes(sig);
		}
	};
	
	struct Combine {
		Signature operator()(Signature const & first, Signature const & second) const {
			return first + second;
		}
		
		template<typename Iterator>
		Signature operator()(Iterator begin, Iterator end) const {
			return sserialize::treeReduce<Iterator, Signature>(begin, end,*this);
		}
	};
	
	struct Resemblence {
		boost::rational<int64_t> operator()(Signature const & first, Signature const & second) const {
			return boost::rational<int64_t>(first / second, Signature::size);
		}
	};
	
	class MayHaveMatch {
	public:
		//This may lead to false negatives since the signature only gives us an estimation.
		//The question is if we can bound the error made by the estimation such that we can rule out false negatives and only produce false positives
		//g = g_u \cup g_\sigma
		//\roh(g_u, g_\sigma) / \roh(g, g_sigma) * |g_\sigma| 
		//
		bool operator()(Signature const & ns) const {
			auto g = m_c(ns, m_ref);
			auto roh_g_ref = m_r(g, m_ref);
			
			if (roh_g_ref == 0) {
				return true;
			}
			else {
				auto roh_ns_ref = m_r(ns, m_ref) * int64_t(m_qg.size());
				auto g_u_int_g_sigma_size = roh_ns_ref/roh_g_ref;
				auto th = int64_t(m_qg.base().size() + m_qg.q() - 1) - int64_t(m_ed * m_qg.q());
				return g_u_int_g_sigma_size >= th;
			}
		}
		auto intersectionSize(Signature const & ns) const {
			auto g = m_c(ns, m_ref);
			auto roh_g_ref = m_r(g, m_ref);
			auto roh_ns_ref = m_r(ns, m_ref) * int64_t(m_qg.size());
			auto g_u_int_g_sigma_size = roh_ns_ref/roh_g_ref;
			return g_u_int_g_sigma_size;
		}
	private:
		friend class MinWiseSignatureTraits;
	private:
		MayHaveMatch(Signature const & ref, QGram const & qg, std::size_t editDistance) :
		m_ref(ref),
		m_qg(qg),
		m_ed(editDistance)
		{}
	private:
		Combine m_c;
		Resemblence m_r;
		Signature m_ref;
		QGram m_qg;
		std::size_t m_ed; //edit distance
	};
	
	class EditDistance {
	public:
		uint32_t operator()(std::string const & other) const {
			
			return 0;
		}
	private:
		friend class MinWiseSignatureTraits;
	private:
		EditDistance(std::string const & base) :
		m_base(base),
		m_t(base.size(), 0)
		{}
	private:
		std::string m_base;
		std::vector<uint32_t> m_t;
	};
	
public:
	MinWiseSignatureTraits() : MinWiseSignatureTraits(3) {}
	MinWiseSignatureTraits(sserialize::UByteArrayAdapter d) {
		d >> m_q >> m_sg;
	}
	MinWiseSignatureTraits(std::size_t q) : MinWiseSignatureTraits(q, 2) {}
	MinWiseSignatureTraits(std::size_t q, std::size_t hashSize) : m_q(q), m_sg(hashSize) {}
	MinWiseSignatureTraits(MinWiseSignatureTraits && other) = default;
	virtual ~MinWiseSignatureTraits() {}
	MinWiseSignatureTraits & operator=(MinWiseSignatureTraits && other) = default;
	uint32_t q() const { return m_q; }
	SignatureGenerator const & sg() const { return m_sg; }
	sserialize::UByteArrayAdapter::SizeType getSizeInBytes() const {
		return sserialize::SerializationInfo<uint32_t>::sizeInBytes(q())
			+ sserialize::SerializationInfo<SignatureGenerator>::sizeInBytes(sg());
	}
public:
	Combine combine() const { return Combine(); }
	MayHaveMatch mayHaveMatch(std::string const & str, std::size_t editDistance) const {
		QGram qg(str, m_q);
		Signature sig = m_sg(qg.begin(), qg.end());
		return MayHaveMatch(sig, qg, editDistance);
	}
	Serializer serializer() const { return Serializer(); }
public:
	Signature signature(std::string const & str) const {
		if (!str.size()) {
			throw sserialize::PreconditionViolationException("Empty string is not allowed!");
		}
		QGram qg(str, m_q);
		return m_sg(qg.begin(), qg.end());
	}
	template<typename T_STRING_ITERATOR>
	Signature signature(T_STRING_ITERATOR begin, T_STRING_ITERATOR end) const {
		if (begin == end) {
			throw sserialize::PreconditionViolationException("Empty string sets are not allowed!");
		}
		Signature sig = signature(*begin);
		for(++begin; begin != end; ++begin) {
			sig += signature(*begin);
		}
		return sig;
	}
private:
	template<std::size_t U, typename V>
	friend sserialize::UByteArrayAdapter & operator<<(sserialize::UByteArrayAdapter & dest, srtree::detail::MinWiseSignatureTraits<U, V> const & v);
	
	template<std::size_t U, typename V>
	friend sserialize::UByteArrayAdapter & operator>>(sserialize::UByteArrayAdapter & dest, srtree::detail::MinWiseSignatureTraits<U, V> & v);
private:
	std::size_t m_q; //the q in q-grams
	SignatureGenerator m_sg;
};

template<std::size_t T_SIZE, typename T_PARAMETRISED_HASH_FUNCTION>
sserialize::UByteArrayAdapter & operator<<(sserialize::UByteArrayAdapter & dest, srtree::detail::MinWiseSignatureTraits<T_SIZE, T_PARAMETRISED_HASH_FUNCTION> const & v) {
	return dest << v.q() << v.sg();
}

template<std::size_t T_SIZE, typename T_PARAMETRISED_HASH_FUNCTION>
sserialize::UByteArrayAdapter & operator>>(sserialize::UByteArrayAdapter & dest, srtree::detail::MinWiseSignatureTraits<T_SIZE, T_PARAMETRISED_HASH_FUNCTION> & v) {
	return dest >> v.m_q >> v.m_sg;
}


}//end namespace srtree::detail
