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
	using StaticTraits = MinWiseSignatureTraits<T_SIZE, T_PARAMETRISED_HASH_FUNCTION>;
public:
	static constexpr std::size_t SignatureSize = T_SIZE;
	using HashFunction = T_PARAMETRISED_HASH_FUNCTION;
	using Signature = MinWiseSignature<SignatureSize, HashFunction::entry_bits>;
	using SignatureGenerator = MinWiseSignatureGenerator<SignatureSize, HashFunction::entry_bits, HashFunction>;
	using QType = uint8_t;
	
	class Serializer {
	public:
		using Type = Signature;
	public:
		inline sserialize::UByteArrayAdapter & operator()(sserialize::UByteArrayAdapter & dest, Signature const & sig) const {
			return dest << sig;
		}
	};
	
	class Deserializer {
	public:
		using Type = Signature;
	public:
		Signature operator()(Type v) const {
			return v;
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
	
	class MayHaveMatch final {
	public:
		MayHaveMatch(MayHaveMatch const &);
		MayHaveMatch(MayHaveMatch &&);
		~MayHaveMatch();
	public:
		bool operator()(Signature const & ns) const;
		MayHaveMatch operator/(MayHaveMatch const & other) const;
		MayHaveMatch operator+(MayHaveMatch const & other) const;
	private:
		struct Node {
			enum Type {LEAF, INTERSECT, UNITE};
			virtual ~Node() {}
			virtual bool matches(MayHaveMatch const & parent, Signature const & v) = 0;
			virtual std::unique_ptr<Node> copy() const = 0;
		};
		class IntersectNode: public Node {
		public:
			IntersectNode(std::unique_ptr<Node> && first, std::unique_ptr<Node> && second);
			~IntersectNode() override;
		public:
			bool matches(MayHaveMatch const & parent, Signature const & v) override;
			std::unique_ptr<Node> copy() const override;
		private:
			std::unique_ptr<Node> first;
			std::unique_ptr<Node> second;
		};
		class UniteNode: public Node {
		public:
			UniteNode(std::unique_ptr<Node> && first, std::unique_ptr<Node> && second);
			~UniteNode() override;
		public:
			bool matches(MayHaveMatch const & parent, Signature const & v) override;
			std::unique_ptr<Node> copy() const override;
		private:
			std::unique_ptr<Node> first;
			std::unique_ptr<Node> second;
		};
		class LeafNode: public Node {
		public:
			LeafNode(Signature const & ref, QGram const & qg, std::size_t editDistance);
			~LeafNode() override;
		public:
			bool matches(MayHaveMatch const & parent, Signature const & v) override;
			std::unique_ptr<Node> copy() const override;
		private:
			Signature m_ref;
			QGram m_qg;
			uint32_t m_ed;
			int32_t m_th;
		};
	private:
		friend class MinWiseSignatureTraits;
	private:
		MayHaveMatch(Signature const & ref, QGram const & qg, std::size_t editDistance);
		MayHaveMatch(std::unique_ptr<Node> && t);
	private:
		Combine m_c;
		Resemblence m_r;
		std::unique_ptr<Node> m_t;
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
	QType q() const { return m_q; }
	SignatureGenerator const & sg() const { return m_sg; }
	sserialize::UByteArrayAdapter::SizeType getSizeInBytes() const {
		return sserialize::SerializationInfo<QType>::sizeInBytes(q())
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
	Deserializer deserializer() const { return Deserializer(); }
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
	QType m_q; //the q in q-grams
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


//implementation
namespace srtree::detail {
	
#define MWSIGTRAITS_TML_HDR template<std::size_t T_SIZE, typename T_PARAMETRISED_HASH_FUNCTION>
#define MWSIGRAMTRAITS_CLS MinWiseSignatureTraits<T_SIZE, T_PARAMETRISED_HASH_FUNCTION>

MWSIGTRAITS_TML_HDR
MWSIGRAMTRAITS_CLS::MayHaveMatch::MayHaveMatch(Signature const & ref, QGram const & qg, std::size_t editDistance) :
m_t( new LeafNode(ref, qg, editDistance) )
{}

MWSIGTRAITS_TML_HDR
MWSIGRAMTRAITS_CLS::MayHaveMatch::MayHaveMatch(std::unique_ptr<Node> && t) :
m_t(std::move(t))
{}

MWSIGTRAITS_TML_HDR
MWSIGRAMTRAITS_CLS::MayHaveMatch::MayHaveMatch(MayHaveMatch const & other) :
m_t(other.m_t->copy())
{}

MWSIGTRAITS_TML_HDR
MWSIGRAMTRAITS_CLS::MayHaveMatch::MayHaveMatch(MayHaveMatch && other) :
m_t(std::move(other.m_t))
{}

MWSIGTRAITS_TML_HDR
MWSIGRAMTRAITS_CLS::MayHaveMatch::~MayHaveMatch() {}

MWSIGTRAITS_TML_HDR
bool
MWSIGRAMTRAITS_CLS::MayHaveMatch::operator()(Signature const & ns) const {
	return m_t->matches(*this, ns);
}

MWSIGTRAITS_TML_HDR
typename MWSIGRAMTRAITS_CLS::MayHaveMatch
MWSIGRAMTRAITS_CLS::MayHaveMatch::operator/(MayHaveMatch const & other) const {
	auto nt = std::unique_ptr<Node>( new IntersectNode(m_t->copy(), other.m_t->copy()) );
	return MayHaveMatch(std::move(nt));
}

MWSIGTRAITS_TML_HDR
typename MWSIGRAMTRAITS_CLS::MayHaveMatch
MWSIGRAMTRAITS_CLS::MayHaveMatch::operator+(MayHaveMatch const & other) const {
	auto nt = std::unique_ptr<Node>( new UniteNode(m_t->copy(), other.m_t->copy()) );
	return MayHaveMatch(std::move(nt));
}

MWSIGTRAITS_TML_HDR
MWSIGRAMTRAITS_CLS::MayHaveMatch::IntersectNode::IntersectNode(std::unique_ptr<Node> && first, std::unique_ptr<Node> && second) :
first(std::move(first)),
second(std::move(second))
{}

MWSIGTRAITS_TML_HDR
MWSIGRAMTRAITS_CLS::MayHaveMatch::IntersectNode::~IntersectNode()
{}

MWSIGTRAITS_TML_HDR
bool
MWSIGRAMTRAITS_CLS::MayHaveMatch::IntersectNode::matches(MayHaveMatch const & parent, Signature const & v) {
	return first->matches(parent, v) && second->matches(parent, v);
}

MWSIGTRAITS_TML_HDR
std::unique_ptr<typename MWSIGRAMTRAITS_CLS::MayHaveMatch::Node>
MWSIGRAMTRAITS_CLS::MayHaveMatch::IntersectNode::copy() const {
	return std::unique_ptr<Node>( new IntersectNode(first->copy(), second->copy()) );
}

MWSIGTRAITS_TML_HDR
MWSIGRAMTRAITS_CLS::MayHaveMatch::UniteNode::UniteNode(std::unique_ptr<Node> && first, std::unique_ptr<Node> && second) :
first(std::move(first)),
second(std::move(second))
{}

MWSIGTRAITS_TML_HDR
MWSIGRAMTRAITS_CLS::MayHaveMatch::UniteNode::~UniteNode() {}

MWSIGTRAITS_TML_HDR
std::unique_ptr<typename MWSIGRAMTRAITS_CLS::MayHaveMatch::Node>
MWSIGRAMTRAITS_CLS::MayHaveMatch::UniteNode::copy() const {
	return std::unique_ptr<Node>( new UniteNode(first->copy(), second->copy()) );
}

MWSIGTRAITS_TML_HDR
bool
MWSIGRAMTRAITS_CLS::MayHaveMatch::UniteNode::matches(MayHaveMatch const & parent, Signature const & v) {
	return first->matches(parent, v) || second->matches(parent, v);
}

MWSIGTRAITS_TML_HDR
MWSIGRAMTRAITS_CLS::MayHaveMatch::LeafNode::LeafNode(Signature const & ref, QGram const & qg, std::size_t editDistance) :
m_ref(ref),
m_qg(qg),
m_ed(editDistance),
m_th(int32_t(m_qg.base().size() + m_qg.q() - 1) - int32_t(m_ed * m_qg.q()))
{}

MWSIGTRAITS_TML_HDR
MWSIGRAMTRAITS_CLS::MayHaveMatch::LeafNode::~LeafNode()
{}

MWSIGTRAITS_TML_HDR
bool
MWSIGRAMTRAITS_CLS::MayHaveMatch::LeafNode::matches(MayHaveMatch const & p, Signature const & ns) {
	//This may lead to false negatives since the signature only gives us an estimation.
	//The question is if we can bound the error made by the estimation such that we can rule out false negatives and only produce false positives
	//g = g_u \cup g_\sigma
	//\roh(g_u, g_\sigma) / \roh(g, g_sigma) * |g_\sigma| 
	//

	auto g = p.m_c(ns, m_ref);
	auto roh_g_ref = p.m_r(g, m_ref);
	
	if (roh_g_ref == 0) {
		return true;
	}
	else {
		auto roh_ns_ref = p.m_r(ns, m_ref) * int64_t(m_qg.size());
		auto g_u_int_g_sigma_size = roh_ns_ref/roh_g_ref;
// 		auto th = int64_t(m_qg.base().size() + m_qg.q() - 1) - int64_t(m_ed * m_qg.q());
		return g_u_int_g_sigma_size >= m_th;
	}
}

MWSIGTRAITS_TML_HDR
std::unique_ptr<typename MWSIGRAMTRAITS_CLS::MayHaveMatch::Node>
MWSIGRAMTRAITS_CLS::MayHaveMatch::LeafNode::copy() const {
	return std::unique_ptr<Node>( new LeafNode(m_ref, m_qg, m_ed) );
}

}//end namespae srtree::detail
