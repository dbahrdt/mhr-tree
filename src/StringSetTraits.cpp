#include <srtree/StringSetTraits.h>

namespace srtree::detail {

StringSetTraits::StringSetTraits() :
m_f(std::make_shared<sserialize::ItemIndexFactory>())
{}

StringSetTraits::StringSetTraits(sserialize::ItemIndexFactory && idxFactory) :
m_f(std::make_shared<sserialize::ItemIndexFactory>(std::move(idxFactory)))
{}

StringSetTraits::StringSetTraits(StringSetTraits && other) : 
m_f(std::move(other.m_f))
{}

StringSetTraits::~StringSetTraits() {}


StringSetTraits::MayHaveMatch::IntersectNode::IntersectNode(std::unique_ptr<Node> && first, std::unique_ptr<Node> && second) :
first(std::move(first)),
second(std::move(second))
{}

StringSetTraits::MayHaveMatch::IntersectNode::~IntersectNode()
{}

bool
StringSetTraits::MayHaveMatch::IntersectNode::matches(sserialize::ItemIndex const & v) {
	return first->matches(v) && second->matches(v);
}

std::unique_ptr<StringSetTraits::MayHaveMatch::Node>
StringSetTraits::MayHaveMatch::IntersectNode::copy() const {
	auto fc = first->copy();
	auto sc = second->copy();
	return std::unique_ptr<Node>( new IntersectNode(std::move(fc), std::move(sc)) );
}

StringSetTraits::MayHaveMatch::UniteNode::UniteNode(std::unique_ptr<Node> && first, std::unique_ptr<Node> && second) :
first(std::move(first)),
second(std::move(second))
{}

StringSetTraits::MayHaveMatch::UniteNode::~UniteNode()
{}

bool
StringSetTraits::MayHaveMatch::UniteNode::matches(sserialize::ItemIndex const & v) {
	return first->matches(v) || second->matches(v);
}

std::unique_ptr<StringSetTraits::MayHaveMatch::Node>
StringSetTraits::MayHaveMatch::UniteNode::copy() const {
	return std::unique_ptr<Node>( new UniteNode(first->copy(), second->copy()) );
}

StringSetTraits::MayHaveMatch::LeafNode::LeafNode(sserialize::ItemIndex const & ref) :
ref(ref)
{}

StringSetTraits::MayHaveMatch::LeafNode::~LeafNode()
{}

bool
StringSetTraits::MayHaveMatch::LeafNode::matches(sserialize::ItemIndex const & v) {
	return (ref / v).size();
}

std::unique_ptr<StringSetTraits::MayHaveMatch::Node>
StringSetTraits::MayHaveMatch::LeafNode::copy() const {
	return std::unique_ptr<Node>( new LeafNode(ref) );
}

StringSetTraits::MayHaveMatch::MayHaveMatch(MayHaveMatch const & other) : m_f(other.m_f), m_t(other.m_t->copy()) {}

StringSetTraits::MayHaveMatch::MayHaveMatch(MayHaveMatch && other) : m_f(std::move(other.m_f)), m_t(std::move(other.m_t)) {}

StringSetTraits::MayHaveMatch::~MayHaveMatch() {}

bool
StringSetTraits::MayHaveMatch::operator()(Signature const & ns) {
	return (*this)(m_f->indexById(ns));
}

bool
StringSetTraits::MayHaveMatch::operator()(sserialize::ItemIndex const & ns) {
	return m_t->matches(ns);
}

StringSetTraits::MayHaveMatch
StringSetTraits::MayHaveMatch::operator/(MayHaveMatch const & other) const {
	auto nt = std::unique_ptr<Node>( new IntersectNode(m_t->copy(), other.m_t->copy()) );
	return MayHaveMatch(m_f, std::move(nt));
}

StringSetTraits::MayHaveMatch
StringSetTraits::MayHaveMatch::operator+(MayHaveMatch const & other) const {
	auto nt = std::unique_ptr<Node>( new UniteNode(m_t->copy(), other.m_t->copy()) );
	return MayHaveMatch(m_f, std::move(nt));
}

StringSetTraits::MayHaveMatch::MayHaveMatch(std::shared_ptr<sserialize::ItemIndexFactory> const & idxFactory, sserialize::ItemIndex const & reference) :
m_f(idxFactory),
m_t(std::unique_ptr<Node>( new LeafNode(reference) ))
{}

StringSetTraits::MayHaveMatch::MayHaveMatch(std::shared_ptr<sserialize::ItemIndexFactory> const & idxFactory, std::unique_ptr<Node> && t) :
m_f(idxFactory),
m_t(std::move(t))
{}
	
}
