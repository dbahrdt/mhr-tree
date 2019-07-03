#pragma once
#include <limits>
#include <sserialize/utility/exceptions.h>
#include <sserialize/storage/UByteArrayAdapter.h>
#include <sserialize/Static/Array.h>
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
	
template<std::size_t T_ENTRY_BITS, typename TEnable = void>
struct EntryType;

template<std::size_t T_ENTRY_BITS>
struct EntryType<T_ENTRY_BITS, typename std::enable_if<bool(T_ENTRY_BITS <= 8)>::type> {
	using type = uint8_t;
};

template<std::size_t T_ENTRY_BITS>
struct EntryType<T_ENTRY_BITS, typename std::enable_if<bool(T_ENTRY_BITS > 8 && T_ENTRY_BITS <= 16)>::type> {
	using type = uint16_t;
};

template<std::size_t T_ENTRY_BITS>
struct EntryType<T_ENTRY_BITS, typename std::enable_if<bool(T_ENTRY_BITS > 16 && T_ENTRY_BITS <= 32)>::type> {
	using type = uint32_t;
};

template<std::size_t T_ENTRY_BITS>
struct EntryType<T_ENTRY_BITS, typename std::enable_if<bool(T_ENTRY_BITS > 32 && T_ENTRY_BITS <= 64)>::type> {
	using type = uint64_t;
};

template<std::size_t T_ENTRY_BITS>
struct EntryType<T_ENTRY_BITS, typename std::enable_if<bool(T_ENTRY_BITS > 64 && T_ENTRY_BITS <= 128)>::type> {
	using type = __uint128_t;
};

//by default we assume that x is iterable
template<std::size_t T_ENTRY_BITS, typename T, typename TEnable = void>
class Converter {
public:
	static constexpr std::size_t entry_bits = T_ENTRY_BITS;
	using entry_type = typename EntryType<T_ENTRY_BITS>::type;
	using value_type = T;
	Converter(entry_type prime) : m_p(prime) {}
	std::size_t operator()(value_type const &) const;
private:
	entry_type m_p;
};

template<std::size_t T_ENTRY_BITS>
class LinearCongruentialHash {
public:
	static constexpr std::size_t entry_bits = T_ENTRY_BITS;
	using size_type = typename EntryType<entry_bits>::type;
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
		m_p = CryptoPP::MaurerProvablePrime(rng, entry_bits-1).ConvertToLong();
	}
	LinearCongruentialHash(sserialize::UByteArrayAdapter d) {
		d >> m_c >> m_p;
	}
	~LinearCongruentialHash() {}
public:
	template<typename T>
	size_type operator()(T const & x) const {
		return (*this)( detail::MinWisePermutation::Converter<entry_bits, T>(m_p)(x) );
	}
	size_type operator()(size_type x) const {
		using computation_type = typename EntryType<entry_bits+2>::type;
		computation_type result = m_c.front();
		for(auto it(m_c.begin()+1), end(m_c.end()); it != end; ++it) {
			result *= x;
			result %= m_p;
			result += *it;
			result %= m_p;
		}
		return result;
	}
private:
	template<std::size_t U>
	friend sserialize::UByteArrayAdapter & operator<<(sserialize::UByteArrayAdapter &, LinearCongruentialHash<U> const &);
private:
	std::vector<size_type> m_c;
	size_type m_p;
};

template<std::size_t U>
sserialize::UByteArrayAdapter & operator<<(sserialize::UByteArrayAdapter & dest, LinearCongruentialHash<U> const & h) {
	return dest << h.m_c << h.m_p;
}

template<typename T_CRYPTOPP_HASH_FUNCTION>
class CryptoPPHash final {
public:
	static constexpr std::size_t entry_bits = 64;
	using size_type = typename EntryType<entry_bits>::type;
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
	CryptoPPHash(sserialize::UByteArrayAdapter const & d) :
	m_c(d.get<size_type>(0))
	{}
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
	template<typename U>
	friend sserialize::UByteArrayAdapter & operator<<(sserialize::UByteArrayAdapter &, CryptoPPHash<U> const &);
private:
	size_type m_c;
};

template<typename T_CRYPTOPP_HASH_FUNCTION>
sserialize::UByteArrayAdapter & operator<<(sserialize::UByteArrayAdapter & dest, CryptoPPHash<T_CRYPTOPP_HASH_FUNCTION> const & h) {
	return dest << h.m_c;
}

} //end namespace detail::MinWisePermutation

template<std::size_t T_SIZE, std::size_t T_ENTRY_BITS>
class MinWiseSignature {
public:
	static constexpr std::size_t size = T_SIZE;
	static constexpr std::size_t entry_bits = T_ENTRY_BITS;
public:
	using size_type = std::size_t;
	using entry_type = typename detail::MinWisePermutation::EntryType<entry_bits>::type;
	using self = MinWiseSignature<size, entry_bits>;
	using container_type = std::array<entry_type, size>;
	using iterator = typename container_type::iterator;
	using const_iterator = typename container_type::const_iterator;
public:
	MinWiseSignature() {
		m_e.fill(std::numeric_limits<entry_type>::max());
	}
	MinWiseSignature(sserialize::UByteArrayAdapter d) {
		for(entry_type & x : m_e) {
			d >> x;
		}
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
	iterator begin() { return m_e.begin(); }
	const_iterator begin() const { return m_e.begin(); }
	const_iterator cbegin() const { return m_e.cbegin(); }
	
	iterator end() { return m_e.end(); }
	const_iterator end() const { return m_e.end(); }
	const_iterator cend() const { return m_e.cend(); }
	
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

template<std::size_t U, std::size_t V>
sserialize::UByteArrayAdapter & operator<<(sserialize::UByteArrayAdapter & dest, MinWiseSignature<U, V> const & sig) {
	for(auto x : sig) {
		dest << x;
	}
	return dest;
}

template<std::size_t T_SIGNATURE_SIZE, std::size_t T_SIGNATURE_ENTRY_BITS, typename T_PARAMETRISED_HASH_FUNCTION = detail::MinWisePermutation::LinearCongruentialHash<T_SIGNATURE_ENTRY_BITS>>
class MinWiseSignatureGenerator {
public:
	static constexpr std::size_t SignatureSize = T_SIGNATURE_SIZE;
	static constexpr std::size_t SignatureEntryBits = T_SIGNATURE_ENTRY_BITS;
public:
	using HashFunction = T_PARAMETRISED_HASH_FUNCTION;
	using Signature = MinWiseSignature<SignatureSize, SignatureEntryBits>;
	using size_type = std::size_t;
public:
	MinWiseSignatureGenerator() : MinWiseSignatureGenerator(2) {}
	MinWiseSignatureGenerator(sserialize::UByteArrayAdapter d) {
		d >> m_perms;
	}
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
	template<std::size_t U, std::size_t V, typename W>
	friend sserialize::UByteArrayAdapter & operator<<(sserialize::UByteArrayAdapter & dest, MinWiseSignatureGenerator<U, V, W>  const & v);
	template<std::size_t U, std::size_t V, typename W>
	friend sserialize::UByteArrayAdapter & operator>>(sserialize::UByteArrayAdapter & dest, MinWiseSignatureGenerator<U, V, W>  & v);
private:
	std::vector<HashFunction> m_perms;
};

template<std::size_t U, std::size_t V, typename W>
sserialize::UByteArrayAdapter & operator<<(sserialize::UByteArrayAdapter & dest, MinWiseSignatureGenerator<U, V, W> const & v) {
	return dest << v.m_perms;
}

template<std::size_t U, std::size_t V, typename W>
sserialize::UByteArrayAdapter & operator>>(sserialize::UByteArrayAdapter & dest, MinWiseSignatureGenerator<U, V, W> & v) {
	return dest >> v.m_perms;
}

namespace detail::MinWisePermutation {
	
//by default we assume that x is iterable
template<std::size_t T_ENTRY_BITS>
class Converter<T_ENTRY_BITS, std::string, void> {
public:
	static constexpr std::size_t entry_bits = T_ENTRY_BITS;
	using value_type = std::string;
	using entry_type = typename EntryType<entry_bits>::type;
	using computation_type = typename EntryType<entry_bits+8+2>::type;
public:
	Converter(std::size_t prime) : m_p(prime) {}
	std::size_t operator()(value_type const & v) {
		computation_type result = 0;
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

template<std::size_t T_ENTRY_BITS, typename T>
class Converter<T_ENTRY_BITS, T, typename std::enable_if<std::is_unsigned<T>::value>::type> {
public:
	static constexpr std::size_t entry_bits = T_ENTRY_BITS;
	using entry_type = typename EntryType<T_ENTRY_BITS>::type;
	using value_type = T;
	Converter(entry_type prime) : m_p(prime) {}
	entry_type operator()(value_type const & v) const {
		return entry_type(v % m_p);
	}
private:
	entry_type m_p;
};

}//end namespace detail::MinWisePermutation

}//end namespace srtree

namespace sserialize {
	
	
template<std::size_t T_SIGNATURE_SIZE, std::size_t T_ENTRY_BITS>
struct SerializationInfo< srtree::MinWiseSignature<T_SIGNATURE_SIZE, T_ENTRY_BITS> > {
	using value_type = srtree::MinWiseSignature<T_SIGNATURE_SIZE, T_ENTRY_BITS>;
	static constexpr bool is_fixed_length = true;
	static constexpr OffsetType length = T_SIGNATURE_SIZE*SerializationInfo<typename value_type::entry_type>::length;
	static constexpr OffsetType max_length = length;
	static constexpr OffsetType min_length = length;
	static constexpr OffsetType sizeInBytes(const value_type & value) {
		return length;
	}
};

} //end namespace sserialize
