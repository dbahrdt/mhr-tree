#include <srtree/Static/StringSetTraits.h>

namespace srtree::Static::detail {

StringSetTraits::StringSetTraits() {}

StringSetTraits::StringSetTraits(sserialize::UByteArrayAdapter const & d) :
m_d(std::make_shared<Data>(d))
{}

StringSetTraits::~StringSetTraits() {}

sserialize::UByteArrayAdapter::SizeType
StringSetTraits::getSizeInBytes() const {
	return m_d->getSizeInBytes();
}

uint32_t
StringSetTraits::strId(std::string const & str) const {
	return m_d->str2Id.at(str, false);
}

StringSetTraits::MayHaveMatch
StringSetTraits::mayHaveMatch(std::string const & str, uint32_t editDistance) const {
	if (editDistance > 0) {
		throw sserialize::UnimplementedFunctionException("StringSetTraits does not support an editDistance > 0 yet.");
	}
	sserialize::ItemIndex strs;
	uint32_t pos = m_d->str2Id.find(str, false);
	if (pos != m_d->str2Id.npos) {
		strs = sserialize::ItemIndex(std::vector<uint32_t>(1, m_d->str2Id.at(pos)));
	}
	return MayHaveMatch(m_d, strs);
}

StringSetTraits::Data::Data(sserialize::UByteArrayAdapter const & d) :
idxStore( sserialize::VersionChecker::check(d, 1, "StringSetTraits") ),
str2Id( d + (1+idxStore.getSizeInBytes()) )
{}

sserialize::UByteArrayAdapter::SizeType
StringSetTraits::Data::getSizeInBytes() const {
	return 1+idxStore.getSizeInBytes()+str2Id.getSizeInBytes();
}

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

StringSetTraits::MayHaveMatch::MayHaveMatch(MayHaveMatch const & other) : m_d(other.m_d), m_t(other.m_t->copy()) {}

StringSetTraits::MayHaveMatch::MayHaveMatch(MayHaveMatch && other) : m_d(std::move(other.m_d)), m_t(std::move(other.m_t)) {}

StringSetTraits::MayHaveMatch::~MayHaveMatch() {}

bool
StringSetTraits::MayHaveMatch::operator()(Signature const & ns) {
	return (*this)(m_d->idxStore.at(ns));
}

bool
StringSetTraits::MayHaveMatch::operator()(sserialize::ItemIndex const & ns) {
	return m_t->matches(ns);
}

StringSetTraits::MayHaveMatch
StringSetTraits::MayHaveMatch::operator/(MayHaveMatch const & other) const {
	auto nt = std::unique_ptr<Node>( new IntersectNode(m_t->copy(), other.m_t->copy()) );
	return MayHaveMatch(m_d, std::move(nt));
}

StringSetTraits::MayHaveMatch
StringSetTraits::MayHaveMatch::operator+(MayHaveMatch const & other) const {
	auto nt = std::unique_ptr<Node>( new UniteNode(m_t->copy(), other.m_t->copy()) );
	return MayHaveMatch(m_d, std::move(nt));
}

StringSetTraits::MayHaveMatch::MayHaveMatch(DataPtr const & d, sserialize::ItemIndex const & reference) :
m_d(d),
m_t(std::unique_ptr<Node>( new LeafNode(reference) ))
{}

StringSetTraits::MayHaveMatch::MayHaveMatch(DataPtr const & d, std::unique_ptr<Node> && t) :
m_d(d),
m_t(std::move(t))
{}


sserialize::UByteArrayAdapter & operator>>(sserialize::UByteArrayAdapter & src, StringSetTraits & traits) {
	sserialize::UByteArrayAdapter tmp(src);
	tmp.shrinkToGetPtr();
	traits = StringSetTraits( tmp );
	src.incGetPtr( sserialize::SerializationInfo<StringSetTraits>::sizeInBytes(traits) );
	return src;
}

	
}//end namespace srtree::Static::detail
