#pragma once
#include <vector>
#include <array>
#include <memory>

#include <sserialize/containers/SimpleBitVector.h>

#include <boost/rational.hpp>
#include <boost/iterator/transform_iterator.hpp>

#include <srtree/GeoRectGeometryTraits.h>
#include <srtree/MinWiseSignatureTraits.h>
#include <srtree/PQGramTraits.h>
#include <srtree/StringSetTraits.h>

#include <srtree/Static/SRTree.h>

namespace srtree {
namespace detail {
	
class NodeOverflowException: public std::exception {};

class Node {
public:
	using self = Node;
	using ptr_type = std::unique_ptr<self>;
	using Boundary = sserialize::spatial::GeoRect;
	using size_type = uint8_t;
	enum Type {INVALID=0, INTERNAL=0x1, LEAF=0x2, ITEM=0x4};
public:
	Node() {}
	virtual ~Node() {}
	virtual Type type() const = 0;
public:
	virtual Boundary const & boundary() const = 0;
public:
	template<typename T>
	T const & as() const {
		SSERIALIZE_NORMAL_ASSERT(dynamic_cast<T const *>(this));
		return static_cast<T const &>(*this);
	}

	template<typename T>
	T & as() {
		SSERIALIZE_NORMAL_ASSERT(dynamic_cast<T *>(this));
		return static_cast<T &>(*this);
	}
};

template<typename T_PAYLOAD>
class NodeWithPayload: public Node {
public:
	using Parent = Node;
	using Boundary = Parent::Boundary;
	using ptr_type = Parent::ptr_type;
	using size_type = Parent::size_type;
	using Type = Parent::Type;
public:
	using Payload = T_PAYLOAD;
public:
	NodeWithPayload() {}
	~NodeWithPayload() override {}
public:
	Payload const & payload() const { return m_payload; }
	Payload & payload() { return m_payload; }
private:
	Payload m_payload;
};

template<typename T_PAYLOAD, uint8_t T_MIN_LOAD, uint8_t T_MAX_LOAD>
class PageNode: public NodeWithPayload<T_PAYLOAD> {
public:
	using Parent = NodeWithPayload<T_PAYLOAD>;
	using Boundary = typename Parent::Boundary;
	using ptr_type = typename Parent::ptr_type;
	using size_type = typename Parent::size_type;
	using Type = typename Parent::Type;
public:
	static constexpr uint8_t MinLoad = T_MIN_LOAD;
	static constexpr uint8_t MaxLoad = T_MAX_LOAD;
public:
	virtual size_type size() const = 0;
	bool isFull() const { return size() >= MaxLoad; }
public:
	virtual void enlarge(Boundary const & other) = 0;
	virtual void recomputeBoundary() = 0;
};

template<typename T_PAYLOAD, uint8_t T_MIN_LOAD, uint8_t T_MAX_LOAD>
class NodeWithChildren: public PageNode<T_PAYLOAD, T_MIN_LOAD, T_MAX_LOAD> {
public:
	using Parent = PageNode<T_PAYLOAD, T_MIN_LOAD, T_MAX_LOAD>;
	using Self = NodeWithChildren<T_PAYLOAD, T_MIN_LOAD, T_MAX_LOAD>;
public:
	using Parent::MinLoad;
	using Parent::MaxLoad;
public:
	using Boundary = typename Parent::Boundary;
	using ptr_type = typename Parent::ptr_type;
	using size_type = typename Parent::size_type;
	using Type = typename Parent::Type;
	using Children = std::array<ptr_type, MaxLoad>;
	using const_iterator = typename Children::const_iterator;
	using iterator = typename Children::iterator;
	using ParentPtr = Node *;
	using ConstParentPtr = Node const *;
public:
	static constexpr size_type npos = std::numeric_limits<size_type>::max();
public:
	NodeWithChildren() : m_s(0), m_p(0) {}
	~NodeWithChildren() override {}
public:
	size_type size() const override {
		return m_s;
	}
	Boundary const & boundary() const override {
		return m_b;
	}
	Boundary const & boundary(size_type p) const {
		return at(p)->boundary();
	}
	ConstParentPtr parent() const {
		return m_p;
	}
	ParentPtr parent() {
		return m_p;
	}
public:
	void enlarge(Boundary const & other) override {
		m_b.enlarge(other);
		if (parent()) {
			parent()->template as<Self>().enlarge(other);
		}
	}
	void recomputeBoundary() override {
		m_b = Boundary();
		for(size_type i(0), s(size()); i < s; ++i) {
			m_b.enlarge(at(i)->boundary());
		}
		if (parent()) {
			parent()->template as<Self>().recomputeBoundary();
		}
	}
	void setParent(ParentPtr p) {
		m_p = p;
	}
public:
	ptr_type const & at(size_type i) const {
		if (m_s <= i) {
			throw std::out_of_range("");
		}
		return m_c.at(i);
	}
	size_type find(Node const * n) const {
		size_type i(0);
		for(auto it(begin()), end(this->end()); it != end; ++it, ++i) {
			if (it->get() == n) {
				return i;
			}
		}
		return npos;
	}
public:
	const_iterator cbegin() const { return children().cbegin(); }
	const_iterator begin() const { return children().begin(); }
	iterator begin() { return children().begin(); }

	const_iterator cend() const { return cbegin()+size(); }
	const_iterator end() const { return begin()+size(); }
	iterator end() { return begin()+size(); }
public:
	virtual void replace(Node const * oldChild, ptr_type && newChild) = 0;
	///prepare replacement of self
	template<typename T_OUTPUT_ITERATOR>
	T_OUTPUT_ITERATOR prepareReplacement(T_OUTPUT_ITERATOR out) {
		for(std::size_t i(0), s(size()); i < s; ++i, ++out) {
			*out = std::move(at(i));
		}
		return out;
	}
	virtual void push_back(ptr_type && child) = 0;
protected:
	void replace_imp(size_type p, ptr_type && child) {
		at(p) = std::move(child);
		recomputeBoundary();
	}
	void push_back_imp(ptr_type && child) {
		if (m_s >= MaxLoad) {
			throw NodeOverflowException();
		}
		enlarge(child->boundary());
		children().at(m_s) = std::move(child);
		++m_s;
	}
	ptr_type & at(size_type i) {
		if (m_s <= i) {
			throw std::out_of_range("");
		}
		return m_c.at(i);
	}
	Children const & children() const { return m_c; }
	Children & children() { return m_c; }
private:
	size_type m_s;
	Boundary m_b;
	ParentPtr m_p;
	Children m_c;
};

template<typename T_PAYLOAD, uint8_t T_MIN_LOAD, uint8_t T_MAX_LOAD>
class InternalNode: public NodeWithChildren<T_PAYLOAD, T_MIN_LOAD, T_MAX_LOAD> {
public:
	using Parent = NodeWithChildren<T_PAYLOAD, T_MIN_LOAD, T_MAX_LOAD>;
	using Self = InternalNode<T_PAYLOAD, T_MIN_LOAD, T_MAX_LOAD>;
public:
	using Parent::MinLoad;
	using Parent::MaxLoad;
public:
	using Boundary = typename Parent::Boundary;
	using ptr_type = typename Parent::ptr_type;
	using size_type = typename Parent::size_type;
	using Type = typename Parent::Type;
	using const_iterator = typename Parent::const_iterator;
	using iterator = typename Parent::iterator;
public:
	InternalNode() {}
	~InternalNode() override {}
	template<typename... Args>
	static std::unique_ptr<Self> make_unique(Args... args) { return std::make_unique<Self>(std::forward<Args>(args)...); }
	Type type() const override { return Node::INTERNAL; }
public:
	void replace(Node const * oldChild, ptr_type && newChild) override {
		newChild->template as<Parent>().setParent(this);
		auto pos = Parent::find(oldChild);
		if (pos != Parent::npos) {
			this->at(pos)->template as<Parent>().setParent(0);
			Parent::replace_imp(pos, std::move(newChild));
		}
		else {
			throw std::out_of_range("oldChild is not a child of this node");
		}
	}
	void push_back(ptr_type && child) override {
		SSERIALIZE_CHEAP_ASSERT_NOT_EQUAL(Node::ITEM, child->type());
		child->template as<Parent>().setParent(this);
		Parent::push_back_imp(std::move(child));
	}
};

template<typename T_PAYLOAD, uint8_t T_MIN_LOAD, uint8_t T_MAX_LOAD>
class LeafNode: public NodeWithChildren<T_PAYLOAD, T_MIN_LOAD, T_MAX_LOAD> {
public:
	using Parent = NodeWithChildren<T_PAYLOAD, T_MIN_LOAD, T_MAX_LOAD>;
	using Self = LeafNode<T_PAYLOAD, T_MIN_LOAD, T_MAX_LOAD>;
public:
	using Parent::MinLoad;
	using Parent::MaxLoad;
public:
	using Boundary = typename Parent::Boundary;
	using ptr_type = typename Parent::ptr_type;
	using size_type = typename Parent::size_type;
	using Type = typename Parent::Type;
	using const_iterator = typename Parent::const_iterator;
	using iterator = typename Parent::iterator;
public:
	LeafNode() {}
	~LeafNode() override {}
	template<typename... Args>
	static std::unique_ptr<Self> make_unique(Args... args) { return std::make_unique<Self>(std::forward<Args>(args)...); }
	Type type() const override { return Node::LEAF; }
public:
	void replace(Node const * oldChild, ptr_type && newChild) override {
		SSERIALIZE_CHEAP_ASSERT_EQUAL(newChild->type(), Node::ITEM);
		Parent::replace_imp(Parent::find(oldChild), std::move(newChild));
	}
	void push_back(ptr_type && child) override {
		SSERIALIZE_CHEAP_ASSERT_EQUAL(child->type(), Node::ITEM);
		Parent::push_back_imp(std::move(child));
	}
};

template<typename T_PAYLOAD, typename T_ITEM>
class ItemNode: public NodeWithPayload<T_PAYLOAD> {
public:
	using Parent = NodeWithPayload<T_PAYLOAD>;
	using Self = ItemNode<T_PAYLOAD, T_ITEM>;
public:
	using Boundary = typename Parent::Boundary;
	using ptr_type = typename Parent::ptr_type;
	using size_type = typename Parent::size_type;
	using Type = typename Parent::Type;
public:
	using item_type = T_ITEM;
public:
	ItemNode(Boundary const & b) : m_b(b) {}
	ItemNode(Boundary const & b, item_type const & item) : m_b(b), m_i(item) {}
	ItemNode(Boundary const & b, item_type && item) : m_b(b), m_i(std::move(item)) {}
	template<typename... Args>
	static std::unique_ptr<Self> make_unique(Args... args) { return std::make_unique<Self>(std::forward<Args>(args)...); }
	~ItemNode() override {}
	Type type() const override { return Node::ITEM; }
public:
	Boundary const & boundary() const override { return m_b; }
public:
	item_type const & item() const { return m_i; }
public:
	Boundary m_b;
	T_ITEM m_i;
};



}//end namespace detail

///Signature provides:
/// operator+() combining two signatures
/// operator/() returning the size of the intersection
/// Planned support for signatures:
/// - Original MHR tree using MinWisePermutations together with q-Grams
/// - ItemIndex: Each node stores the set of strings strings in its subtree, query is then with a set of valid strings
/// - k-min Permutations with q-grams
template<
	typename TSignatureTraits,
	typename TGeometryTraits,
	uint8_t TMinLoad,
	uint8_t TMaxLoad
>
class SRTree {
public:
	static constexpr uint8_t MinLoad = TMinLoad;
	static constexpr uint8_t MaxLoad = TMaxLoad;
	static_assert(MinLoad <= MaxLoad/2);
	static_assert(2 <= MinLoad);
public:
	using SignatureTraits = TSignatureTraits;
	using Signature = typename SignatureTraits::Signature;
	using SignatureMatchPredicate = typename SignatureTraits::MayHaveMatch;
	using SignatureCombine = typename SignatureTraits::Combine;

	using GeometryTraits = TGeometryTraits;
	using Boundary = typename GeometryTraits::Boundary;
	using GeometryMatchPredicate = typename GeometryTraits::MayHaveMatch;
	
	using ItemType = std::size_t;
	using ItemNode = detail::ItemNode<Signature, ItemType>;
	
public:
	struct ItemDescription {
		Boundary boundary;
		Signature signature;
		ItemType item;
	};
public:
	SRTree(SignatureTraits straits = SignatureTraits(), GeometryTraits gtraits = GeometryTraits());
	~SRTree() {}
public:
	inline GeometryTraits const & gtraits() const { return m_gtraits; }
	inline GeometryTraits & gtraits() { return m_gtraits; }
	inline SignatureTraits const & straits() const { return m_straits; }
	inline SignatureTraits & straits() { return m_straits; }
public:
	///Find all items whose boundary intersects with @param b
	template<typename T_OUTPUT_ITERATOR>
	void find(GeometryMatchPredicate smp, T_OUTPUT_ITERATOR out) const;
	
	///Find all items that obey the following conditions:
	///item.boundary.intersect(b) == TRUE
	///size(item.signature.intersect(sig)) >= sigBound
	template<typename T_OUTPUT_ITERATOR>
	void find(GeometryMatchPredicate gmp, SignatureMatchPredicate smp, T_OUTPUT_ITERATOR out) const;
public:
	///@return a pointer to the item node created in the tree which is valid during the lifetime of the tree
	ItemNode const * insert(Boundary const & b, Signature const & sig, ItemType const & item);
	void recalculateSignatures();
public:
	bool checkConsistency() const;
public:
	sserialize::UByteArrayAdapter & serialize(sserialize::UByteArrayAdapter & dest) const;
private:
	using Node = detail::Node;
	using Payload = Signature;
	using NodeWithPayload = detail::NodeWithPayload<Payload>;
	using PageNode = detail::PageNode<Payload, MinLoad, MaxLoad>;
	using NodeWithChildren = detail::NodeWithChildren<Payload, MinLoad, MaxLoad>;
	using InternalNode = detail::InternalNode<Payload, MinLoad, MaxLoad>;
	using LeafNode = detail::LeafNode<Payload, MinLoad, MaxLoad>;
	enum class SplitAxis {LAT, LON, Y=LAT, X=LON};
	struct PayloadDerefer {
		Payload const & operator()(Node::ptr_type const & n) const {
			return (*this)(*n);
		}
		Payload const & operator()(Node const & n) const {
			return n.template as<NodeWithPayload>().payload();
		}
	};
private:
	void splitNode(std::array<Node::ptr_type, MaxLoad+1> & node, NodeWithChildren & fn, NodeWithChildren & sn) const;
	//note that level(m_root) == m_depth, so leafs are in level 0
	void insert(Node::ptr_type && node, std::size_t level);
	//note that level(m_root) == m_depth, so leafs are in level 0
	Node * chooseSubTree(Boundary const & b, std::size_t level) const;
	//note that level(m_root) == m_depth, so leafs are in level 0
	void overflowTreatment(Node* tn, Node::ptr_type&& node, std::size_t level);
private:
	SignatureTraits m_straits;
	GeometryTraits m_gtraits;
	Node::ptr_type m_root;
	std::size_t m_depth{0};
	std::size_t m_rip{MaxLoad/3}; //number of children to reinsert
private:
	sserialize::SimpleBitVector m_ail; //level active dring a insertion
	sserialize::spatial::DistanceCalculator m_dc;
};

template<uint8_t TMinLoad = 12, uint8_t TMaxLoad = 32>
using MHRTree =
	SRTree<
		detail::MinWiseSignatureTraits<64, detail::MinWisePermutation::LinearCongruentialHash>,
		detail::GeoRectGeometryTraits,
		TMinLoad,
		TMaxLoad
	>;
	
template<uint8_t TMinLoad = 12, uint8_t TMaxLoad = 32>
using MSHA3RTree =
	SRTree<
		detail::MinWiseSignatureTraits<64, detail::MinWisePermutation::CryptoPPHash<CryptoPP::SHA3_64>>,
		detail::GeoRectGeometryTraits,
		TMinLoad,
		TMaxLoad
	>;
	
template<uint8_t TMinLoad = 12, uint8_t TMaxLoad = 32>
using QGramRTree =
	SRTree<
		detail::PQGramTraits,
		detail::GeoRectGeometryTraits,
		TMinLoad,
		TMaxLoad
	>;

template<uint8_t TMinLoad = 12, uint8_t TMaxLoad = 32>
using StringSetRTree =
	SRTree<
		detail::StringSetTraits,
		detail::GeoRectGeometryTraits,
		TMinLoad,
		TMaxLoad
	>;

}//end namespace srtree

namespace srtree {
	
#define MHR_TMPL_PARAMS template<typename TSignatureTraits, typename TGeometryTraits, uint8_t TMinLoad, uint8_t TMaxLoad>
#define MHR_CLS_NAME SRTree<TSignatureTraits, TGeometryTraits, TMinLoad, TMaxLoad>

//BEGIN MHRTree

MHR_TMPL_PARAMS
MHR_CLS_NAME::SRTree(SignatureTraits straits, GeometryTraits gtraits) :
m_straits(std::move(straits)),
m_gtraits(std::move(gtraits)),
m_root(LeafNode::make_unique()),
m_dc(sserialize::spatial::DistanceCalculator::DCT_EUCLIDEAN)
{}

MHR_TMPL_PARAMS
template<typename T_OUTPUT_ITERATOR>
void
MHR_CLS_NAME::find(GeometryMatchPredicate gmp, T_OUTPUT_ITERATOR out) const {
	using OutputIterator = T_OUTPUT_ITERATOR;
	struct Recurser {
		GeometryMatchPredicate & gmp;
		OutputIterator & out;
		void operator()(Node const & node) {
			switch (node.type()) {
			case Node::INTERNAL:
			{
				InternalNode const & inode = node.as<InternalNode>();
				for(std::size_t i(0), s(inode.size()); i < s; ++i) {
					if (gmp(inode.at(i)->boundary())) {
						(*this)(*inode.at(i));
					}
				}
			}
				break;
			case Node::LEAF:
			{
				LeafNode const & lnode = node.as<LeafNode>();
				for(std::size_t i(0), s(lnode.size()); i < s; ++i) {
					if (gmp(lnode.at(i)->boundary())) {
						*out = lnode.at(i)->template as<ItemNode>().item();
						++out;
					}
				}
			}
				break;
			case Node::ITEM:
			default:
				break;
			};
		}
		Recurser(GeometryMatchPredicate & gmp, OutputIterator & out) : gmp(gmp), out(out) {}
	};
	if (!m_root || !gmp(m_root->boundary())) {
		return;
	}
	Recurser(gmp, out)(*m_root);
}


MHR_TMPL_PARAMS
template<typename T_OUTPUT_ITERATOR>
void
MHR_CLS_NAME::find(GeometryMatchPredicate gmp, SignatureMatchPredicate smp, T_OUTPUT_ITERATOR out) const {
	using OutputIterator = T_OUTPUT_ITERATOR;
	struct Recurser {
		GeometryMatchPredicate & gmp;
		SignatureMatchPredicate & smp;
		OutputIterator & out;
		void operator()(Node const & node) {
			switch (node.type()) {
			case Node::INTERNAL:
			{
				InternalNode const & inode = node.as<InternalNode>();
				for(std::size_t i(0), s(inode.size()); i < s; ++i) {
					if (gmp(inode.at(i)->boundary()) && smp(inode.at(i)->template as<NodeWithPayload>().payload())) {
						(*this)(*inode.at(i));
					}
				}
			}
				break;
			case Node::LEAF:
			{
				LeafNode const & lnode = node.as<LeafNode>();
				for(std::size_t i(0), s(lnode.size()); i < s; ++i) {
					if (gmp(lnode.at(i)->boundary()) && smp(lnode.at(i)->template as<NodeWithPayload>().payload())) {
						*out = lnode.at(i)->template as<ItemNode>().item();
						++out;
					}
				}
			}
				break;
			case Node::ITEM:
			default:
				break;
			};
		}
		Recurser(GeometryMatchPredicate & gmp, SignatureMatchPredicate & smp, OutputIterator & out) :
		gmp(gmp), smp(smp), out(out)
		{}
	};
	if (!m_root || !gmp(m_root->boundary())) {
		return;
	}
	Recurser(gmp, smp, out)(*m_root);
}

MHR_TMPL_PARAMS
typename MHR_CLS_NAME::ItemNode const *
MHR_CLS_NAME::insert(Boundary const & b, Signature const & sig, ItemType const & item) {
	auto in = ItemNode::make_unique(b, item);
	ItemNode const * result = in.get();
	in->payload() = sig;
	m_ail.reset();
	insert(std::move(in), 0);
	SSERIALIZE_EXPENSIVE_ASSERT( checkConsistency() );
	return result;
}

MHR_TMPL_PARAMS
void
MHR_CLS_NAME::insert(std::unique_ptr<Node> && node, std::size_t level) {
	SSERIALIZE_EXPENSIVE_ASSERT( checkConsistency() );
	Node * tn = chooseSubTree(node->boundary(), level);
	if (!tn->as<PageNode>().isFull()) {
		tn->as<NodeWithChildren>().push_back(std::move(node));
	}
	else {
		overflowTreatment(tn, std::move(node), level);
	}
	SSERIALIZE_EXPENSIVE_ASSERT( checkConsistency() );
}

MHR_TMPL_PARAMS
void
MHR_CLS_NAME::overflowTreatment(Node * tn, Node::ptr_type && node, std::size_t level) {
	SSERIALIZE_EXPENSIVE_ASSERT( checkConsistency() );
	bool doSplit = m_ail.isSet(level) || level == m_depth;
	m_ail.set(level);
	if (doSplit) {
		Node::ptr_type fn, sn;
		if (tn->type() == Node::LEAF) {
			fn = LeafNode::make_unique();
			sn = LeafNode::make_unique();
		}
		else {
			fn = InternalNode::make_unique();
			sn = InternalNode::make_unique();
		}
		std::array<Node::ptr_type, MaxLoad+1> tnc;
		tn->as<NodeWithChildren>().prepareReplacement(tnc.begin());
		tnc.back() = std::move(node);
		
		splitNode(tnc, fn->as<NodeWithChildren>(), sn->as<NodeWithChildren>());
		Node * tnp = tn->as<NodeWithChildren>().parent();
		if (!tnp) {// root node was split
			SSERIALIZE_CHEAP_ASSERT_EQUAL(m_depth, level);
			SSERIALIZE_CHEAP_ASSERT_EQUAL(tn, m_root.get());
			m_root = InternalNode::make_unique();
			m_root->as<InternalNode>().push_back(std::move(fn));
			m_root->as<InternalNode>().push_back(std::move(sn));
			m_depth += 1;
		}
		else if (!tnp->as<NodeWithChildren>().isFull()) { //parent has enough room
			tnp->as<NodeWithChildren>().replace(tn, std::move(fn));
			tnp->as<NodeWithChildren>().push_back(std::move(sn));
		}
		else {
			tnp->as<NodeWithChildren>().replace(tn, std::move(fn));
			overflowTreatment(tnp, std::move(sn), level+1);
		}
	}
	else { //reinsert
		Node::ptr_type nn;
		if (tn->type() == Node::LEAF) {
			nn = LeafNode::make_unique();
		}
		else {
			nn = InternalNode::make_unique();
		}
		
		sserialize::spatial::GeoPoint center(tn->boundary().midLat(), tn->boundary().midLon());
		std::array<Node::ptr_type, MaxLoad+1> tnc;
		tn->as<NodeWithChildren>().prepareReplacement(tnc.begin());
		tnc.back() = std::move(node);
		
		std::array<std::size_t, MaxLoad+1> tmp;
		std::array<double, MaxLoad+1> dists;
		for(std::size_t i(0); i < MaxLoad+1; ++i) {
			dists[i] = m_dc.calc(tnc[i]->boundary().midLat(), tnc[i]->boundary().midLon(), center.lat(), center.lon());
			tmp[i] = i;
		}
		std::sort(tmp.begin(), tmp.end(), [&dists](std::size_t a, std::size_t b) {
			return dists[a] > dists[b];
		});
		for(std::size_t i(m_rip); i < MaxLoad+1; ++i) {
			nn->as<NodeWithChildren>().push_back(std::move(tnc.at(tmp[i])));
		}
		tn->as<NodeWithChildren>().parent()->template as<NodeWithChildren>().replace(tn, std::move(nn));
		//reinsert the remaining children, these are the ones that are far apart
		for(std::size_t i(0); i < m_rip; ++i) {
			insert(std::move(tnc.at(tmp[i])), level);
		}
	}
	SSERIALIZE_EXPENSIVE_ASSERT( checkConsistency() );
}

MHR_TMPL_PARAMS
typename MHR_CLS_NAME::Node *
MHR_CLS_NAME::chooseSubTree(Boundary const & b, std::size_t level) const {
	SSERIALIZE_CHEAP_ASSERT_SMALLER_OR_EQUAL(level, m_depth);
	
	auto overlap = [](Boundary const & b, auto begin, auto end) {
		double result = 0;
		for(; begin != end; ++begin) {
			result += (b / (*begin)->boundary()).area();
		}
		return result;
	};
	auto areaEnlargement = [](Boundary const & base, Boundary const & toAdd) {
		return base.enlarged(toAdd).area() - base.area();
	};
	Node * cn = m_root.get();
	std::size_t clvl = m_depth;
	while (clvl > level) {
		InternalNode const & n = cn->as<InternalNode>();
		if (n.at(0)->type() == Node::LEAF) {
			SSERIALIZE_CHEAP_ASSERT_EQUAL(clvl-1, 0);
			std::size_t bestPos = std::numeric_limits<std::size_t>::max();
			double bestOverlap = std::numeric_limits<double>::max();
			double bestAreaEnlargement = std::numeric_limits<double>::max();
			for(std::size_t i(0), s(n.size()); i < s; ++i) {
				auto const & myln = n.at(i)->template as<LeafNode>();
				double ov = overlap(b, myln.begin(), myln.end());
				if (ov < bestOverlap) {
					bestPos = i;
					bestOverlap = ov;
					bestAreaEnlargement = areaEnlargement(myln.boundary(), b);
				}
				else if (ov == bestOverlap) {
					double ae = areaEnlargement(myln.boundary(), b);
					if (ae < bestAreaEnlargement) {
						bestPos = i;
					}
				}
			}
			cn = n.at(bestPos).get();
			--clvl;
		}
		else {
			SSERIALIZE_CHEAP_ASSERT_EQUAL(Node::INTERNAL, n.template as<InternalNode>().at(0)->type());
			std::size_t bestPos = std::numeric_limits<std::size_t>::max();
			double bestAreaEnlargement = std::numeric_limits<double>::max();
			for(std::size_t i(0), s(n.size()); i < s; ++i) {
				auto const & myin = n.at(i)->template as<InternalNode>();
				double ae = areaEnlargement(myin.boundary(), b);
				if (ae < bestAreaEnlargement) {
					bestAreaEnlargement = ae;
					bestPos = i;
				}
			}
			cn = n.at(bestPos).get();
			--clvl;
		}
	}
	return cn;
}

MHR_TMPL_PARAMS
void
MHR_CLS_NAME::recalculateSignatures() {
	
	struct Recurser {
		using PayloadIterator = boost::transform_iterator<PayloadDerefer, typename NodeWithChildren::const_iterator>;
		Recurser(SignatureCombine combine) : combine(combine) {}
		void operator()(Node & node) {
			switch(node.type()) {
			case Node::INTERNAL:
			{
				InternalNode & in = node.as<InternalNode>();
				for(auto it(in.begin()), end(in.end()); it != end; ++it) {
					(*this)(**it);
				}
				in.payload() = combine(PayloadIterator(in.begin()), PayloadIterator(in.end()));
			}
				break;
			case Node::LEAF:
			{
				LeafNode & lf = node.as<LeafNode>();
				lf.payload() = combine(PayloadIterator(lf.begin()), PayloadIterator(lf.end()));
			}
				break;
			default:
				break;
			};
		}
		SignatureCombine combine;
	};
	if (m_root) {
		Recurser(straits().combine())(*m_root);
	}
}

MHR_TMPL_PARAMS
bool
MHR_CLS_NAME::checkConsistency() const {
	struct Recurser {
		SRTree const * d;
		Recurser(SRTree const * d) : d(d) {}
		bool operator()() {
			return (*this)(*(d->m_root), d->m_depth);
		}
		bool operator()(Node const & node, uint32_t lvl) {
			SSERIALIZE_CHEAP_ASSERT_SMALLER_OR_EQUAL(lvl, d->m_depth);
			if (lvl == 0) {
				if (node.type() != Node::LEAF) {
					SSERIALIZE_CHEAP_ASSERT_EQUAL(Node::LEAF, node.type());
					return false;
				}
				NodeWithChildren const & n = node.as<NodeWithChildren>();
				Boundary b;
				for(std::size_t i(0), s(n.size()); i < s; ++i) {
					if (n.at(i)->boundary() != n.boundary(i)) {
						SSERIALIZE_CHEAP_ASSERT_EQUAL(n.at(i)->boundary(), n.boundary(i));
						return false;
					}
					b.enlarge(n.boundary(i));
				}
				for(std::size_t i(0), s(n.size()); i < s; ++i) {
					if (n.at(i)->type() != Node::ITEM) {
						SSERIALIZE_CHEAP_ASSERT_EQUAL(Node::ITEM, n.at(i)->type());
						return false;
					}
				}
				if (b != n.boundary()) {
					SSERIALIZE_CHEAP_ASSERT_EQUAL(b, n.boundary());
					return false;
				}
			}
			else {
				if (node.type() != Node::INTERNAL) {
					SSERIALIZE_CHEAP_ASSERT_EQUAL(Node::INTERNAL, node.type());
					return false;
				}
				NodeWithChildren const & n = node.as<NodeWithChildren>();
				for(std::size_t i(0), s(n.size()); i < s; ++i) {
					if (n.at(i)->template as<NodeWithChildren>().parent() != &node) {
						SSERIALIZE_CHEAP_ASSERT_EQUAL(&node, n.at(i)->template as<NodeWithChildren>().parent());
						return false;
					}
				}
				int childrenTypes = 0;
				for(std::size_t i(0), s(n.size()); i < s; ++i) {
					childrenTypes |= n.at(i)->type();
					if (sserialize::popCount(childrenTypes) != 1) {
						SSERIALIZE_CHEAP_ASSERT_EQUAL(1, sserialize::popCount(childrenTypes));
						return false;
					}
				}
				for(std::size_t i(0), s(n.size()); i < s; ++i) {
					if (!(*this)( *(node.template as<NodeWithChildren>().at(i)), lvl-1)) {
						return false;
					}
				}
			}
			return true;
		};
	};
	return Recurser(this)();
}


MHR_TMPL_PARAMS
sserialize::UByteArrayAdapter &
MHR_CLS_NAME::serialize(sserialize::UByteArrayAdapter & dest) const {
	
	dest << 1; //version
	dest << m_depth; //meta-data
	
	using SSelf = srtree::Static::SRTree<SignatureTraits, GeometryTraits>;
	
	sserialize::Static::ArrayCreator<typename SSelf::Node> nac(dest);
	std::cout << "SRTree: Serializing nodes..." << std::flush;
	std::vector<Node const *> nodes;
	for(std::size_t i(0); i < nodes.size(); ++i) {
		Node const & node = * (nodes.at(i));
		switch(node.type()) {
		case Node::INTERNAL:
		{
			InternalNode const & in = node.as<InternalNode>();
			nac.put( typename SSelf::Node(nodes.size(), in.size()) );
			for(auto it(in.begin()), end(in.end()); it != end; ++it) {
				nodes.push_back(it->get());
			}
		}
			break;
		case Node::LEAF:
		{
			LeafNode const & lf = node.as<LeafNode>();
			nac.put( typename SSelf::Node(nodes.size(), lf.size()) );
			for(auto it(lf.begin()), end(lf.end()); it != end; ++it) {
				nodes.push_back(it->get());
			}
		}
			break;
		default:
			break;
		};
	}
	nac.flush();
	std::cout << "done" << std::endl;
	
	std::cout << "SRTree: Serializing boundaries..." << std::flush;
	sserialize::Static::ArrayCreator<Boundary> bac(dest);
	for(auto n : nodes) {
		bac.put( n->boundary() );
	}
	bac.flush();
	std::cout << "done" << std::endl;
	
	std::cout << "SRTree: Serializing signatures..." << std::flush;
	sserialize::Static::ArrayCreator<Signature> sac(dest);
	for(auto n : nodes) {
		sac.put( n->template as<NodeWithPayload>().payload() );
	}
	sac.flush();
	std::cout << "done" << std::endl;
	
	std::cout << "SRTree: Serializing items..." << std::flush;
	sserialize::Static::ArrayCreator<ItemType> iac(dest);
	for(auto n : nodes) {
		if (n->type() == Node::ITEM) {
			iac.put( n->template as<ItemNode>().item() );
		}
	}
	iac.flush();
	std::cout << "done" << std::endl;
	return dest;
}

MHR_TMPL_PARAMS
void
MHR_CLS_NAME::splitNode(std::array<Node::ptr_type, MaxLoad+1> & node, NodeWithChildren & fn, NodeWithChildren & sn) const {
	
	struct OverlapArea {
		double o{std::numeric_limits<double>::max()};
		double a{std::numeric_limits<double>::max()};
		bool operator<(OverlapArea const & other) const {
			return o == other.o ? a < other.a : o < other.o;
		}
	};
	
	auto minValue = [&node](std::array<std::size_t, MaxLoad+1> const & sort, auto quantity, auto initial) {
		std::size_t minValuePosition = MinLoad;
		for(std::size_t i(MinLoad); (MaxLoad+1-i) >= MinLoad; ++i) { //the different groupings with each node having at least MinLoad elements
			Boundary fgBounds, sgBounds;
			std::size_t j(0);
			for(; j < i; ++j) { //first group
				fgBounds.enlarge(node.at(sort.at(j))->boundary());
			}
			for(; j < MaxLoad+1; ++j) { //second group
				sgBounds.enlarge(node.at(sort.at(j))->boundary());
			}
			auto q = quantity(fgBounds, sgBounds);
			if (q < initial) {
				q = initial;
				minValuePosition = i;
			}
		}
		return std::make_pair(initial, minValuePosition);
	};
	
	auto quantityMargin = [](Boundary const & fgBounds, Boundary const & sgBounds) {
		return fgBounds.lengthInM()+sgBounds.lengthInM();
	};
	auto quantityOverlapArea = [](Boundary const & fgBounds, Boundary const & sgBounds) {
		OverlapArea oa;
		oa.o = (fgBounds / sgBounds).area();
		oa.a = fgBounds.area()+sgBounds.area();
		return oa;
	};
	
	enum {LATS_MIN=0, LATS_MAX=1, LONS_MIN=2, LONS_MAX=3};
	std::array<std::array<std::size_t, MaxLoad+1>, 4> corners;
	for(std::size_t i(0); i < 4; ++i) {
		for(std::size_t j(0); j < MaxLoad+1; ++j) {
			corners[i][j] = j;
		}
	}
	
	std::sort(corners[LATS_MIN].begin(), corners[LATS_MIN].end(), [&node](std::size_t a, std::size_t b) {
		return node.at(a)->boundary().minLat() < node.at(b)->boundary().minLat();
	});
	std::sort(corners[LATS_MAX].begin(), corners[LATS_MAX].end(), [&node](std::size_t a, std::size_t b) {
		return node.at(a)->boundary().maxLat() < node.at(b)->boundary().maxLat();
	});
	std::sort(corners[LONS_MIN].begin(), corners[LONS_MIN].end(), [&node](std::size_t a, std::size_t b) {
		return node.at(a)->boundary().minLon() < node.at(b)->boundary().minLon();
	});
	std::sort(corners[LONS_MAX].begin(), corners[LONS_MAX].end(), [&node](std::size_t a, std::size_t b) {
		return node.at(a)->boundary().maxLon() < node.at(b)->boundary().maxLon();
	});
	
	//BEGIN ChooseSplitAxis
	std::array<double, 4> margins; //best margins for each sort
	for(std::size_t k(0); k < 4; ++k) {
		margins[k] = minValue(corners[k], quantityMargin, std::numeric_limits<double>::max()).first;
	}
	auto m = std::min_element(margins.begin(), margins.end()) - margins.begin();
	//END ChooseSplitAxis
	
	//BEGIN ChooseSplitIndex
	auto splitIndex = minValue(corners.at(m), quantityOverlapArea, OverlapArea());
	//END ChooseSplitIndex
	
	std::size_t i(0);
	for(; i < splitIndex.second; ++i) {
		fn.push_back(std::move(node.at(corners.at(m).at(i))));
	}
	for(; i < MaxLoad+1; ++i) {
		sn.push_back(std::move(node.at(corners.at(m).at(i))));
	}
}

#undef MHR_TMPL_PARAMS
#undef MHR_CLS_NAME

//END MHRTRree


}//end namespace srtree
