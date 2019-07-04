#pragma once

#include <liboscar/StaticOsmCompleter.h>
#include <sserialize/containers/HashBasedFlatTrie.h>
#include <srtree/SRTree.h>

struct OStringSetRTree {
	using Tree = srtree::StringSetRTree<12, 32>;
	using SignatureTraits = Tree::SignatureTraits;
	using Signature = Tree::Signature;
	
	struct StringId {
		static constexpr uint32_t Invalid = std::numeric_limits<uint32_t>::max();
		static constexpr uint32_t Internal = Invalid-1;
		static constexpr uint32_t GenericLeaf = Internal-1;
		
		bool valid() const { return value != Invalid; }
		bool internal() const { return value == Internal; }
		bool leaf() const { return value < Internal; }
		
		uint32_t value{Invalid};
	};
	
	struct State {
		Tree tree;
		sserialize::HashBasedFlatTrie<StringId> str2Id;
		std::vector<Tree::ItemNode const *> itemNodes;
	};
	struct CreationState {
		///in ghId order!
		std::vector<sserialize::ItemIndex> regionStrIds;
		std::vector<sserialize::ItemIndex> cellStrIds;
		sserialize::SimpleBitVector processedItems;
		SignatureTraits::Combine combine;
	public:
		CreationState(SignatureTraits const & straits) : combine(straits.combine()) {}
	};
public:
	OStringSetRTree(std::shared_ptr<liboscar::Static::OsmCompleter> cmp) :
	cmp(cmp), cstate(state.tree.straits())
	{}
public:
	void setCheck(bool check) { this->check = check; }
public:
	void init();
	void create();
public:
	void serialize(sserialize::UByteArrayAdapter & treeData, sserialize::UByteArrayAdapter & traitsData);
public:
	void test();
	sserialize::ItemIndex cellStrIds(uint32_t cellId);
	sserialize::ItemIndex itemStrIds(uint32_t itemId);
	std::string normalize(std::string const & str);
private:
	sserialize::ItemIndex matchingStrings(std::string const & str, bool prefixMatch);
	sserialize::ItemIndex matchingItems(typename Tree::GeometryMatchPredicate & gmp, typename Tree::SignatureMatchPredicate & smp);
public:
	std::shared_ptr<liboscar::Static::OsmCompleter> cmp;
	State state;
	CreationState cstate;
	bool check{false};
	
};
