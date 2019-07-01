#pragma once
#include <limits>
#include <sserialize/utility/exceptions.h>
#include <random>
#include <crypto++/osrng.h>
#include <crypto++/nbtheory.h>
#include <crypto++/sha3.h>

namespace CryptoPP {
	
class SHA3_64 : public SHA3
{
public:
	CRYPTOPP_CONSTANT(DIGESTSIZE = 8)

	//! \brief Construct a SHA3-64 message digest
	SHA3_64() : SHA3(DIGESTSIZE) {}
	CRYPTOPP_CONSTEXPR static const char *StaticAlgorithmName() {return "SHA3-64";}
};
	
}//end namespace CryptoPP

namespace srtree {
namespace detail::MinWisePermutation {

//by default we assume that x is iterable
template<typename T>
class Converter {
	using value_type = T;
	Converter(std::size_t prime) : m_p(prime) {}
	std::size_t operator()(value_type const & v);
private:
	std::size_t m_p;
};

class LinearCongruentialHash {
public:
	using size_type = std::size_t;
public:
	LinearCongruentialHash(CryptoPP::RandomNumberGenerator & rng, size_type size) {
		m_c.reserve(size);
		for(std::size_t i(0); i < size; ++i) {
			std::array<unsigned char, sizeof(size_type)> tmp;
			rng.GenerateBlock(tmp.data(), tmp.size());
			size_type tmp2;
			::memmove(&tmp2, tmp.data(), sizeof(tmp2));
			m_c.push_back(tmp2);
		}
		m_p = CryptoPP::MaurerProvablePrime(rng, 63).ConvertToLong();
	}
	~LinearCongruentialHash() {}
public:
	template<typename T>
	size_type operator()(T const & x) const {
		return (*this)( detail::MinWisePermutation::Converter<T>(m_p)(x) );
	}
	size_type operator()(size_type x) const {
		using uint128_t = __uint128_t;
		uint128_t result = m_c.front();
		for(auto it(m_c.begin()+1), end(m_c.end()); it != end; ++it) {
			result *= x;
			result %= m_p;
			result += *it;
			result %= m_p;
		}
		return result;
	}
private:
	std::vector<size_type> m_c;
	size_type m_p;
};


template<typename T_CRYPTOPP_HASH_FUNCTION>
class CryptoPPHash final {
public:
	using size_type = std::size_t;
	using HashFunction = T_CRYPTOPP_HASH_FUNCTION;
public:
	CryptoPPHash(CryptoPP::RandomNumberGenerator & rng, size_type /*size*/) {
		std::array<unsigned char, sizeof(size_type)> tmp;
		rng.GenerateBlock(tmp.data(), tmp.size());
		size_type tmp2;
		::memmove(&tmp2, tmp.data(), sizeof(tmp2));
		m_c = tmp2;
	}
	CryptoPPHash(CryptoPPHash const & other) : m_c(other.m_c) {}
	~CryptoPPHash() {}
public:
	size_type operator()(std::string const & str) const {
		std::array<unsigned char, sizeof(size_type)> tmp;
		HashFunction h;
		h.Update((unsigned char *)(&m_c), sizeof(m_c));
		h.Update((unsigned char *)(str.c_str()), str.size());
		h.TruncatedFinal(tmp.data(), sizeof(size_type));
		size_type tmp2;
		::memmove(&tmp2, tmp.data(), sizeof(tmp2));
		return tmp2;
	}
	size_type operator()(size_type x) const {
		std::array<unsigned char, sizeof(size_type)> tmp;
		HashFunction h;
		h.Update((unsigned char *)(&m_c), sizeof(m_c));
		h.Update((unsigned char *)(&x), sizeof(x));
		h.TruncatedFinal(tmp.data(), sizeof(size_type));
		size_type tmp2;
		::memmove(&tmp2, tmp.data(), sizeof(tmp2));
		return tmp2;
	}
private:
	size_type m_c;
};

} //end namespace detail::MinWisePermutation

template<std::size_t T_SIZE>
class MinWiseSignature {
public:
	static constexpr std::size_t size = T_SIZE;
public:
	using size_type = std::size_t;
	using entry_type = uint64_t;
	using self = MinWiseSignature<size>;
public:
	MinWiseSignature() {
		m_e.fill(std::numeric_limits<entry_type>::max());
	}
	MinWiseSignature(std::initializer_list<entry_type> const & l) :
	MinWiseSignature(l.begin(), l.end())
	{}
	template<typename T_ITERATOR>
	MinWiseSignature(T_ITERATOR begin, T_ITERATOR end) {
		using std::distance;
		if (distance(begin, end) <= size) {
			throw sserialize::AllocationException("Range too small");
		}
		for(std::size_t i(0); i < size; ++i, ++begin) {
			m_e.at(i) = *begin;
		}
	}
	~MinWiseSignature() {}
public:
	entry_type const & at(size_type i) const {
		return m_e.at(i);
	}
	entry_type & at(size_type i) {
		return m_e.at(i);
	}
	self operator+(self const & other) const {
		MinWiseSignature result(*this);
		result += other;
		return result;
	}
	self & operator+=(self const & other) {
		for(size_type i(0); i < size; ++i) {
			using std::min;
			at(i) = min(at(i), other.at(i));
		}
		return *this;
		
	}
	size_type operator/(self const & other) const {
		size_type result = 0;
		for(size_type i(0); i < size; ++i) {
			if (this->at(i) == other.at(i)) {
				result += 1;
			}
		}
		return result;
	}
	bool operator==(self const & other) const {
		return m_e == other.m_e;
	}
	bool operator!=(self const & other) const {
		return m_e != other.m_e;
	}
private:
	std::array<entry_type, size> m_e;
};

template<std::size_t T_SIGNATURE_SIZE, typename T_PARAMETRISED_HASH_FUNCTION = detail::MinWisePermutation::LinearCongruentialHash>
class MinWiseSignatureGenerator {
public:
	static constexpr std::size_t SignatureSize = T_SIGNATURE_SIZE;
public:
	using Signature = MinWiseSignature<SignatureSize>;
	using size_type = std::size_t;
	using HashFunction = T_PARAMETRISED_HASH_FUNCTION;
public:
	MinWiseSignatureGenerator() : MinWiseSignatureGenerator(2) {}
	MinWiseSignatureGenerator(size_type hashSize) {
		CryptoPP::AutoSeededRandomPool rng;
		init(rng, hashSize);
	}
	MinWiseSignatureGenerator(CryptoPP::RandomNumberGenerator & rng, size_type hashSize) {
		init(rng, hashSize);
	}
public:
	template<typename T>
	Signature operator()(T const & x) const {
		Signature sig;
		for(size_type i(0); i < SignatureSize; ++i) {
			sig.at(i) = m_perms.at(i)(x);
		}
		return sig;
	}
	
	template<typename T_ITERATOR>
	Signature operator()(T_ITERATOR begin, T_ITERATOR end) const {
		using std::min;
		Signature sig = (*this)(*begin);
		for (++begin; begin != end; ++begin) {
			for(size_type i(0); i < SignatureSize; ++i) {
				sig.at(i) = min(sig.at(i), m_perms.at(i)(*begin));
			}
		}
		return sig;
	}
private:
	void init(CryptoPP::RandomNumberGenerator & rng, size_type hashSize) {
		m_perms.reserve(SignatureSize);
		for(size_type i(0); i < SignatureSize; ++i) {
			m_perms.emplace_back(rng, hashSize);
		}
	}
	
private:
	std::vector<HashFunction> m_perms;
};

namespace detail::MinWisePermutation {

//by default we assume that x is iterable
template<>
class Converter<std::string> {
public:
	using value_type = std::string;
public:
	Converter(std::size_t prime) : m_p(prime) {}
	inline std::size_t operator()(value_type const & v) {
		using uint128_t = __uint128_t;
		uint128_t result = 0;
		for(auto it(v.rbegin()), end(v.rend()); it != end; ++it) {
			result <<= 8;
			result += uint8_t(*it);
			result %= m_p;
		}
		return result;
	}
private:
	std::size_t m_p;
};

}//end namespace detail::MinWisePermutation

}//end namespace srtree
