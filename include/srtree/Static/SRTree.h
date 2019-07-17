#pragma once

#include <limits>

#include <sserialize/storage/UByteArrayAdapter.h>
#include <sserialize/Static/Array.h>
#include <sserialize/iterator/RangeGenerator.h>
#include <sserialize/storage/SerializationInfo.h>
#include <sserialize/Static/Version.h>

#include <srtree/MinWiseSignatureTraits.h>
#include <srtree/GeoRectGeometryTraits.h>

namespace srtree::Static {
namespace detail {
/**
 * struct Node {
 *   uint32_t firstChild;
 *   uint8_t numChildren
 * };
 */
class Node final {
public:
	using ChildrenContainer = sserialize::RangeGenerator<uint32_t>;
	using const_iterator = ChildrenContainer::const_iterator;
public:
	Node() : Node(0, 0) {}
	Node(uint32_t firstChild, uint8_t numChildren) :
	m_children(firstChild, firstChild+numChildren, 1)
	{}
	Node(sserialize::UByteArrayAdapter const & d) :
	m_children(d.getUint32(0), d.getUint32(0) + d.getUint8(4), 1)
	{}
	~Node() {}
	sserialize::UByteArrayAdapter::SizeType getSizeInBytes() const { return 5; }
public:
	inline uint32_t size() const { return m_children.size(); }
	inline const_iterator begin() const { return m_children.begin(); }
	inline const_iterator end() const { return m_children.end(); }
private:
	ChildrenContainer m_children;
};

inline sserialize::UByteArrayAdapter operator<<(sserialize::UByteArrayAdapter & dest, Node const & node) {
	return dest << uint32_t(*node.begin()) << uint8_t(node.size());
}

}//end namespace detail
}//end namespace srtree::Static

namespace sserialize {
	
template<>
struct SerializationInfo<srtree::Static::detail::Node> {
	static const bool is_fixed_length = true;
	static const OffsetType length = 5;
	static const OffsetType max_length = 5;
	static const OffsetType min_length = 5;
	static OffsetType sizeInBytes(const srtree::Static::detail::Node &) {
		return 5;
	}
};

} //end namespace sserialize

namespace srtree::Static::detail {

/**
 * struct MetaData {
 *   u32 depth;
 *   u32 numInternalNodes;
 *   u32 numLeafNodes;
 *   u32 numItemNodes;
 * };
 * 
 */
class MetaData {
public:
	MetaData() {}
	MetaData(sserialize::UByteArrayAdapter d) :
	m_depth(d.getUint32()),
	m_numInternalNodes(d.getUint32()),
	m_numLeafNodes(d.getUint32()),
	m_numItemNodes(d.getUint32())
	{}
	sserialize::UByteArrayAdapter::SizeType getSizeInBytes() const { return sserialize::SerializationInfo<uint32_t>::length*4; }
public:
	inline uint32_t depth() const { return m_depth; }
	inline uint32_t numInternalNodes() const { return m_numInternalNodes; }
	inline uint32_t numLeafNodes() const { return m_numLeafNodes; }
	inline uint32_t numItemNodes() const { return m_numItemNodes; }
private:
	uint32_t m_depth{0};
	uint32_t m_numInternalNodes{0};
	uint32_t m_numLeafNodes{0};
	uint32_t m_numItemNodes{0};
};
	
}//end namespace srtree::Static::detail

//tree is serialized in level order

namespace srtree::Static {

/**
 * struct SRTree: Version(2) {
 *   MetaData m_md;
 *   Array<Node> m_nodes; //internal and leaf nodes, no item nodes
 *   Array<Boundary> m_bds;
 *   Array<Signature> m_sigs;
 *   Array<ItemType> m_items;
 * };
 */
template<
	typename TSignatureTraits,
	typename TGeometryTraits
>
class SRTree final: sserialize::Static::SimpleVersion<2>  {
public:
	
	using Version = sserialize::Static::SimpleVersion<2>;
	
	static constexpr uint32_t SignatureSize = 56;
	static constexpr uint32_t nid = std::numeric_limits<uint32_t>::max();
	
	using Node = srtree::Static::detail::Node;
	
	using ItemType = uint32_t;
	
	using SignatureTraits = TSignatureTraits;
	using Signature = typename SignatureTraits::Signature;
	using SignatureMatchPredicate = typename SignatureTraits::MayHaveMatch;
	
	using GeometryTraits = TGeometryTraits;
	using Boundary = typename GeometryTraits::Boundary;
	using GeometryMatchPredicate = typename GeometryTraits::MayHaveMatch;
	
	class MetaNode {
	public:
		//compatible with srtree::detail::Node::Type
		enum Type {INVALID=0, INTERNAL=0x1, LEAF=0x2, ITEM=0x4};
	public:
		Type type() const {
			if (m_nid < m_p->m_md.numInternalNodes()) {
				return INTERNAL;
			}
			else if (m_nid < m_p->m_md.numInternalNodes()+m_p->m_md.numLeafNodes()) {
				return LEAF;
			}
			else {
				return ITEM;
			}
		}
		uint32_t id() const {
			return m_nid;
		}
		Boundary boundary() const {
			return m_p->boundary(m_nid);
		}
		Signature signature() const {
			return m_p->signature(m_nid);
		}
		ItemType item() const {
			return m_p->item(m_nid);
		}
		uint32_t numberOfChildren() const {
			return m_p->node(m_nid).size();
		}
		MetaNode child(uint32_t pos) const {
			return MetaNode(m_p, *(m_p->node(m_nid).begin()+pos));
		}
	private:
		friend class SRTree;
	private:
		MetaNode(SRTree const * parent, uint32_t nodeId) :
		m_p(parent),
		m_nid(nodeId)
		{}
	private:
		SRTree const * m_p;
		uint32_t m_nid;
	};
	using MetaData = detail::MetaData;
	
public:
	SRTree() {}
	SRTree(sserialize::UByteArrayAdapter d, SignatureTraits straits = SignatureTraits(), GeometryTraits gtraits = GeometryTraits());
	SRTree(SRTree && other);
	~SRTree() {}
	SRTree & operator=(SRTree && other);
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

	///Visit all nodes obeying the following conditions:
	///item.boundary.intersect(b) == TRUE
	///size(item.signature.intersect(sig)) >= sigBound
	///@param out iterator accepting a MetaNode
	template<typename T_OUTPUT_ITERATOR>
	void visit(GeometryMatchPredicate gmp, SignatureMatchPredicate smp, T_OUTPUT_ITERATOR out) const;
	
public:
	MetaNode root() const { return MetaNode(this, 0); }
	MetaData const & metaData() const { return m_md; }
private:
	enum Type { INTERNAL_NODE, LEAF_NODE};
	using Level = int;
private:
	Type type(Level level) const;
	Node node(uint32_t nodeId) const;
	Boundary boundary(uint32_t nodeId) const;
	Signature signature(uint32_t nodeId) const;
	ItemType item(uint32_t nodeId) const;
public:
	SignatureTraits m_straits;
	GeometryTraits m_gtraits;
	MetaData m_md;
	sserialize::Static::Array<Node> m_nodes;
	sserialize::Static::Array<typename GeometryTraits::Deserializer::Type> m_bds;
	sserialize::Static::Array<typename SignatureTraits::Deserializer::Type> m_sigs;
	sserialize::Static::Array<ItemType> m_items;
};

}//end namespace srtree::Static


//Implementation
namespace srtree::Static {
	
#define MHR_TMPL_PARAMS template<typename TSignatureTraits, typename TGeometryTraits>
#define MHR_CLS_NAME SRTree<TSignatureTraits, TGeometryTraits>

MHR_TMPL_PARAMS
MHR_CLS_NAME::SRTree(sserialize::UByteArrayAdapter d, SignatureTraits straits, GeometryTraits gtraits) :
Version(d, Version::Consume()),
m_straits(std::move(straits)),
m_gtraits(std::move(gtraits)),
m_md(d)
{
	d += m_md.getSizeInBytes();
	d >> m_nodes >> m_bds >> m_sigs >> m_items;
}

MHR_TMPL_PARAMS
MHR_CLS_NAME::SRTree(SRTree && other) :
m_straits(std::move(other.m_straits)),
m_gtraits(std::move(other.m_gtraits)),
m_md(std::move(other.m_md)),
m_nodes(std::move(other.m_nodes)),
m_bds(std::move(other.m_bds)),
m_sigs(std::move(other.m_sigs)),
m_items(std::move(other.m_items))
{}

MHR_TMPL_PARAMS
MHR_CLS_NAME &
MHR_CLS_NAME::operator=(SRTree && other) {
	m_straits = std::move(other.m_straits);
	m_gtraits = std::move(other.m_gtraits);
	m_md = std::move(other.m_md);
	m_nodes = std::move(other.m_nodes);
	m_bds = std::move(other.m_bds);
	m_sigs = std::move(other.m_sigs);
	m_items = std::move(other.m_items);
	return *this;
}

MHR_TMPL_PARAMS
template<typename T_OUTPUT_ITERATOR>
void
MHR_CLS_NAME::find(GeometryMatchPredicate gmp, T_OUTPUT_ITERATOR out) const {
	using OutputIterator = T_OUTPUT_ITERATOR;
	struct Recurser {
		SRTree const & that;
		GeometryMatchPredicate & gmp;
		OutputIterator & out;
		void operator()(uint32_t nodeId, Level level) {
			Node node = that.node(nodeId);
			switch (that.type(level)) {
			case INTERNAL_NODE:
			{
				for(uint32_t childId : node) {
					if (gmp( that.boundary(childId) )) {
						(*this)(childId, level-1);
					}
				}
			}
				break;
			case LEAF_NODE:
			{
				for(uint32_t childId : node) {
					if ( gmp( that.boundary(childId) ) ) {
						*out = that.item(childId);
						++out;
					}
				}
			}
			default:
				break;
			};
		}
		Recurser(SRTree const & that, GeometryMatchPredicate & gmp, OutputIterator & out) :
		that(that), gmp(gmp), out(out)
		{}
	};
	if (!m_nodes.size()) {
		return;
	}
	Recurser(*this, gmp, out)(0, m_md.depth());
}

MHR_TMPL_PARAMS
template<typename T_OUTPUT_ITERATOR>
void
MHR_CLS_NAME::find(GeometryMatchPredicate gmp, SignatureMatchPredicate smp, T_OUTPUT_ITERATOR out) const {
	using OutputIterator = T_OUTPUT_ITERATOR;
	struct Recurser {
		SRTree const & that;
		GeometryMatchPredicate & gmp;
		SignatureMatchPredicate & smp;
		OutputIterator & out;
		void operator()(uint32_t nodeId, Level level) {
			Node node = that.node(nodeId);
			switch (that.type(level)) {
			case INTERNAL_NODE:
			{
				for(uint32_t childId : node) {
					if ( gmp( that.boundary(childId) ) && smp(that.signature(childId)) ) {
						(*this)(childId, level-1);
					}
				}
			}
				break;
			case LEAF_NODE:
			{
				for(uint32_t childId : node) {
					if ( gmp( that.boundary(childId) ) && smp(that.signature(childId)) ) {
						*out = that.item(childId);
						++out;
					}
				}
			}
			default:
				break;
			};
		}
		Recurser(SRTree const & that, GeometryMatchPredicate & gmp, SignatureMatchPredicate & smp, OutputIterator & out) :
		that(that), gmp(gmp), smp(smp), out(out)
		{}
	};
	if (!m_nodes.size()) {
		return;
	}
	Recurser(*this, gmp, smp, out)(0, m_md.depth());
}

MHR_TMPL_PARAMS
template<typename T_OUTPUT_ITERATOR>
void
MHR_CLS_NAME::visit(GeometryMatchPredicate gmp, SignatureMatchPredicate smp, T_OUTPUT_ITERATOR out) const {
	using OutputIterator = T_OUTPUT_ITERATOR;
	struct Recurser {
		SRTree const & that;
		GeometryMatchPredicate & gmp;
		SignatureMatchPredicate & smp;
		OutputIterator & out;
		void operator()(uint32_t nodeId, Level level) {
			*out = MetaNode(&that, nodeId);
			++out;
			Node node = that.node(nodeId);
			switch (that.type(level)) {
			case INTERNAL_NODE:
			{
				for(uint32_t childId : node) {
					if ( gmp( that.boundary(childId) ) && smp(that.signature(childId)) ) {
						(*this)(childId, level-1);
					}
				}
			}
				break;
			case LEAF_NODE:
			{
				for(uint32_t childId : node) {
					if ( gmp( that.boundary(childId) ) && smp(that.signature(childId)) ) {
						*out = MetaNode(&that, childId);
						++out;
					}
				}
			}
			default:
				break;
			};
		}
		Recurser(SRTree const & that, GeometryMatchPredicate & gmp, SignatureMatchPredicate & smp, OutputIterator & out) :
		that(that), gmp(gmp), smp(smp), out(out)
		{}
	};
	if (!m_nodes.size()) {
		return;
	}
	Recurser(*this, gmp, smp, out)(0, m_md.depth());
}

MHR_TMPL_PARAMS
typename MHR_CLS_NAME::Type
MHR_CLS_NAME::type(Level level) const {
	if (level == 0) {
		return LEAF_NODE;
	}
	else {
		return INTERNAL_NODE;
	}
}

MHR_TMPL_PARAMS
typename MHR_CLS_NAME::Node
MHR_CLS_NAME::node(uint32_t nodeId) const {
	return m_nodes.at(nodeId);
}

MHR_TMPL_PARAMS
typename MHR_CLS_NAME::Boundary
MHR_CLS_NAME::boundary(uint32_t nodeId) const {
	return gtraits().deserializer()( m_bds.at(nodeId) );
}

MHR_TMPL_PARAMS
typename MHR_CLS_NAME::Signature
MHR_CLS_NAME::signature(uint32_t nodeId) const {
	return straits().deserializer()( m_sigs.at(nodeId) );
}

MHR_TMPL_PARAMS
typename MHR_CLS_NAME::ItemType
MHR_CLS_NAME::item(uint32_t nodeId) const {
	return m_items.at( nodeId - (m_md.numInternalNodes()+m_md.numLeafNodes()) );
}

#undef MHR_TMPL_PARAMS
#undef MHR_CLS_NAME
	
} //end namespace srtree::Static
