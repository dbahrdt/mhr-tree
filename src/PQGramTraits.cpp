#include <srtree/PQGramTraits.h>

namespace srtree::detail {
	
PQGramTraits::MayHaveMatch::MayHaveMatch(MayHaveMatch const & other) :
m_d(other.m_d),
m_t(other.m_t->copy())
{}

PQGramTraits::MayHaveMatch::MayHaveMatch(MayHaveMatch && other) :
m_d(std::move(other.m_d)),
m_t(std::move(other.m_t))
{}

PQGramTraits::MayHaveMatch::~MayHaveMatch() {}

bool
PQGramTraits::MayHaveMatch::operator()(Signature const & ns) const {
	return !m_t->nomatch(*m_d, ns, m_ed);
}

PQGramTraits::MayHaveMatch
PQGramTraits::MayHaveMatch::operator/(MayHaveMatch const & other) const {
	auto nt = std::unique_ptr<Node>( new IntersectNode(m_t->copy(), other.m_t->copy()) );
	return MayHaveMatch(m_d, std::move(nt), m_ed);
}

PQGramTraits::MayHaveMatch
PQGramTraits::MayHaveMatch::operator+(MayHaveMatch const & other) const {
	auto nt = std::unique_ptr<Node>( new UniteNode(m_t->copy(), other.m_t->copy()) );
	return MayHaveMatch(m_d, std::move(nt), m_ed);
}

PQGramTraits::MayHaveMatch::MayHaveMatch(std::shared_ptr<::srtree::PQGramDB> const & d, MatchReference const & reference, uint32_t ed) :
m_d(d),
m_t(std::unique_ptr<Node>( new LeafNode(reference) )),
m_ed(ed)
{}

PQGramTraits::MayHaveMatch::IntersectNode::IntersectNode(std::unique_ptr<Node> && first, std::unique_ptr<Node> && second) :
first(std::move(first)),
second(std::move(second))
{}

PQGramTraits::MayHaveMatch::IntersectNode::~IntersectNode()
{}

bool
PQGramTraits::MayHaveMatch::IntersectNode::nomatch(srtree::PQGramDB const & db, Signature const & v, uint32_t ed) {
	return first->nomatch(db, v, ed) || second->nomatch(db, v, ed);
}

std::unique_ptr<PQGramTraits::MayHaveMatch::Node>
PQGramTraits::MayHaveMatch::IntersectNode::copy() const {
	return std::unique_ptr<Node>( new IntersectNode(first->copy(), second->copy()) );
}

PQGramTraits::MayHaveMatch::UniteNode::UniteNode(std::unique_ptr<Node> && first, std::unique_ptr<Node> && second) :
first(std::move(first)),
second(std::move(second))
{}

PQGramTraits::MayHaveMatch::UniteNode::~UniteNode() {}

std::unique_ptr<PQGramTraits::MayHaveMatch::Node>
PQGramTraits::MayHaveMatch::UniteNode::copy() const {
	return std::unique_ptr<Node>( new UniteNode(first->copy(), second->copy()) );
}

bool
PQGramTraits::MayHaveMatch::UniteNode::nomatch(srtree::PQGramDB const & db, Signature const & v, uint32_t ed) {
	return first->nomatch(db, v, ed) && second->nomatch(db, v, ed);
}

PQGramTraits::MayHaveMatch::LeafNode::LeafNode(MatchReference const & ref) :
ref(ref)
{}

PQGramTraits::MayHaveMatch::LeafNode::~LeafNode()
{}

bool
PQGramTraits::MayHaveMatch::LeafNode::nomatch(srtree::PQGramDB const & db, Signature const & v, uint32_t ed) {
	return db.nomatch(v, ref, ed);
}

std::unique_ptr<PQGramTraits::MayHaveMatch::Node>
PQGramTraits::MayHaveMatch::LeafNode::copy() const {
	return std::unique_ptr<Node>( new LeafNode(ref) );
}

PQGramTraits::MayHaveMatch::MayHaveMatch(std::shared_ptr<::srtree::PQGramDB> const & d, std::unique_ptr<Node> && t, uint32_t ed) :
m_d(d),
m_t(std::move(t)),
m_ed(ed)
{}

PQGramTraits::PQGramTraits(uint32_t q) :
m_d(new srtree::PQGramDB(q))
{}

PQGramTraits::Combine
PQGramTraits::combine() const {
	return Combine();
}

PQGramTraits::MayHaveMatch
PQGramTraits::mayHaveMatch(std::string const & ref, uint32_t ed) const {
	return MayHaveMatch(m_d, m_d->find(ref), ed);
}

void
PQGramTraits::add(const std::string & str) {
	m_d->insert(str);
}

PQGramTraits::Signature
PQGramTraits::signature(const std::string & str) {
	return m_d->find(str);
}
	
}//end namespace
