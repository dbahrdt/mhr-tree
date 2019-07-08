#pragma once

#include <srtree/QGram.h>
#include <srtree/QGramDB.h>

#include <sserialize/algorithm/utilfunctional.h>
#include <sserialize/storage/SerializationInfo.h>

#include <memory>

namespace srtree::detail {
	
class PQGramTraitsBase {
public:
	using Signature = PQGramDB::PQGramSet;
	
	class Serializer {
	public:
		inline sserialize::UByteArrayAdapter & operator()(sserialize::UByteArrayAdapter & dest, Signature const & v) const {
			return dest << v;
		}
	};

	class Combine {
	public:
		inline Signature operator()(Signature const & first, Signature const & second) const {
			return first + second;
		}
		template<typename Iterator>
		inline Signature operator()(Iterator begin, Iterator end) const {
			return sserialize::treeReduce<Iterator, Signature>(begin, end, *this);
		}
	};
	
public:
	virtual ~PQGramTraitsBase() {}
public:
	inline Combine combine() const { return Combine(); }
	inline Serializer serializer() const { return Serializer(); }
};
	
template<typename T_PQGRAMDB>
class ROPQGramTraits: public PQGramTraitsBase{
public:
	using Parent = PQGramTraitsBase;
	using PQGramDB = T_PQGRAMDB;
	
	class MayHaveMatch {
	public:
		using MatchReference = Signature;
	public:
		MayHaveMatch(MayHaveMatch const & other);
		MayHaveMatch(MayHaveMatch && other);
		~MayHaveMatch();
	public:
		bool operator()(Signature const & ns) const;
		MayHaveMatch operator/(MayHaveMatch const & other) const;
		MayHaveMatch operator+(MayHaveMatch const & other) const;
	private:
		friend class ROPQGramTraits;
	private:
		MayHaveMatch(std::shared_ptr<PQGramDB> const & db, MatchReference const & reference, uint32_t ed);
	private:
		struct Node {
			enum Type {LEAF, INTERSECT, UNITE};
			virtual ~Node() {}
			virtual bool nomatch(PQGramDB const & db, Signature const & v, uint32_t ed) = 0;
			virtual std::unique_ptr<Node> copy() const = 0;
		};
		struct IntersectNode: public Node {
			std::unique_ptr<Node> first;
			std::unique_ptr<Node> second;
			bool nomatch(PQGramDB const & db, Signature const & v, uint32_t ed) override;
			IntersectNode(std::unique_ptr<Node> && first, std::unique_ptr<Node> && second);
			~IntersectNode() override;
			std::unique_ptr<Node> copy() const override;
		};
		struct UniteNode: public Node {
			std::unique_ptr<Node> first;
			std::unique_ptr<Node> second;
			~UniteNode() override;
			bool nomatch(PQGramDB const & db, Signature const & v, uint32_t ed) override;
			
			UniteNode(std::unique_ptr<Node> && first, std::unique_ptr<Node> && second);
			std::unique_ptr<Node> copy() const override;
		};
		struct LeafNode: public Node {
			~LeafNode() override;
			bool nomatch(PQGramDB const & db, Signature const & v, uint32_t ed) override;
			LeafNode(MatchReference const & ref);
			std::unique_ptr<Node> copy() const override;
			MatchReference ref;
		};
	private:
		MayHaveMatch(std::shared_ptr<PQGramDB> const & d, std::unique_ptr<Node> && t, uint32_t ed);
	private:
		bool nomatch(Signature const & v);
	private:
		std::shared_ptr<PQGramDB> m_d;
		std::unique_ptr<Node> m_t;
		uint32_t m_ed;
	};
public:
	ROPQGramTraits()
	{}
	ROPQGramTraits(PQGramDB const & db) :
	m_d(std::make_shared<PQGramDB>(db))
	{}
	ROPQGramTraits(PQGramDB && db) :
	m_d(std::make_shared<PQGramDB>(std::move(db)))
	{}
	ROPQGramTraits(std::shared_ptr<PQGramDB> const & db) :
	m_d(db)
	{}
	ROPQGramTraits(ROPQGramTraits const &) = default;
	ROPQGramTraits(ROPQGramTraits && other) = default;
	~ROPQGramTraits() override {}

	ROPQGramTraits & operator=(ROPQGramTraits const &) = default;
	ROPQGramTraits & operator=(ROPQGramTraits &&) = default;
public:
	MayHaveMatch mayHaveMatch(std::string const & ref, uint32_t ed) const;
public:
	Signature signature(const std::string & str);
public:
	PQGramDB & db() { return *m_d; }
	PQGramDB const & db() const { return *m_d; }
private:
	std::shared_ptr<PQGramDB> m_d;
};

class PQGramTraits: public ROPQGramTraits<::srtree::PQGramDB> {
public:
	using Parent = ROPQGramTraits<::srtree::PQGramDB>;
public:
	PQGramTraits(uint32_t q = 3) : Parent(PQGramDB(q)) {}
	PQGramTraits(PQGramTraits const &) = default;
	PQGramTraits(PQGramTraits && other) = default;
	~PQGramTraits() override {}
	PQGramTraits & operator=(PQGramTraits const &) = default;
	PQGramTraits & operator=(PQGramTraits &&) = default;
public:
	void add(const std::string & str);
};

inline sserialize::UByteArrayAdapter & operator<<(sserialize::UByteArrayAdapter & dest, PQGramTraits const & v) {
	return dest << v.db();
}
}//end namespace srtree::detail

namespace srtree::Static::detail {

class PQGramTraits: public srtree::detail::ROPQGramTraits<::srtree::Static::PQGramDB> {
public:
	using Parent = srtree::detail::ROPQGramTraits<::srtree::Static::PQGramDB>;
public:
	PQGramTraits() {}
	PQGramTraits(PQGramDB const & db) : Parent(db) {}
	PQGramTraits(PQGramDB && db) : Parent(db) {}
	PQGramTraits(sserialize::UByteArrayAdapter const & d) : Parent(PQGramDB(d)) {}
	PQGramTraits(PQGramTraits const &) = default;
	PQGramTraits(PQGramTraits && other) = default;
	~PQGramTraits() override {}
	PQGramTraits & operator=(PQGramTraits const &) = default;
	PQGramTraits & operator=(PQGramTraits &&) = default;
public:
	inline sserialize::UByteArrayAdapter::SizeType getSizeInBytes() const {
		return sserialize::SerializationInfo<PQGramDB>::sizeInBytes(db());
	}
};

sserialize::UByteArrayAdapter & operator>>(sserialize::UByteArrayAdapter & src, PQGramTraits & dest);

} //end namespace srtree::Static::detail

//implementation

namespace srtree::detail {
	
#define ROPQGRAMTRAITS_TML_HDR template<typename T_PQGRAMDB>
#define ROPQGRAMTRAITS_CLS ROPQGramTraits<T_PQGRAMDB>

ROPQGRAMTRAITS_TML_HDR
ROPQGRAMTRAITS_CLS::MayHaveMatch::MayHaveMatch(MayHaveMatch const & other) :
m_d(other.m_d),
m_t(other.m_t->copy())
{}

ROPQGRAMTRAITS_TML_HDR
ROPQGRAMTRAITS_CLS::MayHaveMatch::MayHaveMatch(MayHaveMatch && other) :
m_d(std::move(other.m_d)),
m_t(std::move(other.m_t))
{}

ROPQGRAMTRAITS_TML_HDR
ROPQGRAMTRAITS_CLS::MayHaveMatch::~MayHaveMatch() {}

ROPQGRAMTRAITS_TML_HDR
bool
ROPQGRAMTRAITS_CLS::MayHaveMatch::operator()(Signature const & ns) const {
	return !m_t->nomatch(*m_d, ns, m_ed);
}

ROPQGRAMTRAITS_TML_HDR
typename ROPQGRAMTRAITS_CLS::MayHaveMatch
ROPQGRAMTRAITS_CLS::MayHaveMatch::operator/(MayHaveMatch const & other) const {
	auto nt = std::unique_ptr<Node>( new IntersectNode(m_t->copy(), other.m_t->copy()) );
	return MayHaveMatch(m_d, std::move(nt), m_ed);
}

ROPQGRAMTRAITS_TML_HDR
typename ROPQGRAMTRAITS_CLS::MayHaveMatch
ROPQGRAMTRAITS_CLS::MayHaveMatch::operator+(MayHaveMatch const & other) const {
	auto nt = std::unique_ptr<Node>( new UniteNode(m_t->copy(), other.m_t->copy()) );
	return MayHaveMatch(m_d, std::move(nt), m_ed);
}

ROPQGRAMTRAITS_TML_HDR
ROPQGRAMTRAITS_CLS::MayHaveMatch::MayHaveMatch(std::shared_ptr<PQGramDB> const & d, MatchReference const & reference, uint32_t ed) :
m_d(d),
m_t(std::unique_ptr<Node>( new LeafNode(reference) )),
m_ed(ed)
{}

ROPQGRAMTRAITS_TML_HDR
ROPQGRAMTRAITS_CLS::MayHaveMatch::IntersectNode::IntersectNode(std::unique_ptr<Node> && first, std::unique_ptr<Node> && second) :
first(std::move(first)),
second(std::move(second))
{}

ROPQGRAMTRAITS_TML_HDR
ROPQGRAMTRAITS_CLS::MayHaveMatch::IntersectNode::~IntersectNode()
{}

ROPQGRAMTRAITS_TML_HDR
bool
ROPQGRAMTRAITS_CLS::MayHaveMatch::IntersectNode::nomatch(PQGramDB const & db, Signature const & v, uint32_t ed) {
	return first->nomatch(db, v, ed) || second->nomatch(db, v, ed);
}

ROPQGRAMTRAITS_TML_HDR
std::unique_ptr<typename ROPQGRAMTRAITS_CLS::MayHaveMatch::Node>
ROPQGRAMTRAITS_CLS::MayHaveMatch::IntersectNode::copy() const {
	return std::unique_ptr<Node>( new IntersectNode(first->copy(), second->copy()) );
}

ROPQGRAMTRAITS_TML_HDR
ROPQGRAMTRAITS_CLS::MayHaveMatch::UniteNode::UniteNode(std::unique_ptr<Node> && first, std::unique_ptr<Node> && second) :
first(std::move(first)),
second(std::move(second))
{}

ROPQGRAMTRAITS_TML_HDR
ROPQGRAMTRAITS_CLS::MayHaveMatch::UniteNode::~UniteNode() {}

ROPQGRAMTRAITS_TML_HDR
std::unique_ptr<typename ROPQGRAMTRAITS_CLS::MayHaveMatch::Node>
ROPQGRAMTRAITS_CLS::MayHaveMatch::UniteNode::copy() const {
	return std::unique_ptr<Node>( new UniteNode(first->copy(), second->copy()) );
}

ROPQGRAMTRAITS_TML_HDR
bool
ROPQGRAMTRAITS_CLS::MayHaveMatch::UniteNode::nomatch(PQGramDB const & db, Signature const & v, uint32_t ed) {
	return first->nomatch(db, v, ed) && second->nomatch(db, v, ed);
}

ROPQGRAMTRAITS_TML_HDR
ROPQGRAMTRAITS_CLS::MayHaveMatch::LeafNode::LeafNode(MatchReference const & ref) :
ref(ref)
{}

ROPQGRAMTRAITS_TML_HDR
ROPQGRAMTRAITS_CLS::MayHaveMatch::LeafNode::~LeafNode()
{}

ROPQGRAMTRAITS_TML_HDR
bool
ROPQGRAMTRAITS_CLS::MayHaveMatch::LeafNode::nomatch(PQGramDB const & db, Signature const & v, uint32_t ed) {
	return db.nomatch(v, ref, ed);
}

ROPQGRAMTRAITS_TML_HDR
std::unique_ptr<typename ROPQGRAMTRAITS_CLS::MayHaveMatch::Node>
ROPQGRAMTRAITS_CLS::MayHaveMatch::LeafNode::copy() const {
	return std::unique_ptr<Node>( new LeafNode(ref) );
}

ROPQGRAMTRAITS_TML_HDR
ROPQGRAMTRAITS_CLS::MayHaveMatch::MayHaveMatch(std::shared_ptr<PQGramDB> const & d, std::unique_ptr<Node> && t, uint32_t ed) :
m_d(d),
m_t(std::move(t)),
m_ed(ed)
{}

ROPQGRAMTRAITS_TML_HDR
typename ROPQGRAMTRAITS_CLS::MayHaveMatch
ROPQGRAMTRAITS_CLS::mayHaveMatch(std::string const & ref, uint32_t ed) const {
	return MayHaveMatch(m_d, m_d->find(ref), ed);
}

ROPQGRAMTRAITS_TML_HDR
PQGramTraitsBase::Signature
ROPQGRAMTRAITS_CLS::signature(const std::string & str) {
	return m_d->find(str);
}

#undef ROPQGRAMTRAITS_TML_HDR
#undef ROPQGRAMTRAITS_CLS
	
}//end namespace srtree::detail
