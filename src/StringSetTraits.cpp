#include <srtree/StringSetTraits.h>

namespace srtree::detail {

StringSetTraits::StringSetTraits() :
m_d(std::make_shared<Data>())
{}

StringSetTraits::StringSetTraits(sserialize::ItemIndexFactory && idxFactory) :
m_d(std::make_shared<Data>(std::move(idxFactory)))
{}

StringSetTraits::StringSetTraits(StringSetTraits && other) : 
m_d(std::move(other.m_d))
{}

StringSetTraits::~StringSetTraits() {}


void
StringSetTraits::addString(std::string const & str) {
	str2Id().insert(str);
}

void
StringSetTraits::finalizeStringTable() {
	//mark all strings as leafs
	for(auto it(str2Id().begin()), end(str2Id().end()); it != end; ++it) {
		it->second.value = StringId::GenericLeaf;
	}
	
	//this will add internal nodes
	str2Id().finalize();
	
	//mark all newly created nodes as internal, the other nodes get increasing ids
	{
		uint32_t i{0};
		for(auto it(str2Id().begin()), end(str2Id().end()); it != end; ++it, ++i) {
			if (!it->second.valid()) {
				it->second.value = StringId::Internal;
			}
			else {
				it->second.value = i;
			}
		}
	}
}

uint32_t
StringSetTraits::strId(std::string const & str) const {
	return str2Id().at(str).value;
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
	return (*this)(m_d->idxFactory.indexById(ns));
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

sserialize::UByteArrayAdapter & operator<<(sserialize::UByteArrayAdapter & dest, StringSetTraits & traits) {
	dest << sserialize::Static::SimpleVersion<1>(); //version;
	traits.idxFactory().flush();
	dest.put(traits.idxFactory().getFlushedData());
	traits.str2Id().append(dest, [](StringSetTraits::String2IdMap::NodePtr const & n) -> uint32_t {
		return n->value().value;
	},
	1);
	return dest;
}
	
}//end namespace srtree::detail
