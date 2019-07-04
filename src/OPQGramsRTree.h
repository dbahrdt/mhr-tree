#pragma once

#include <liboscar/StaticOsmCompleter.h>
#include <srtree/SRTree.h>

struct OPQGramsRTree {
	using Tree = srtree::QGramRTree<12, 32>;
	using SignatureTraits = Tree::SignatureTraits;
	using Signature = Tree::Signature;
	struct State {
		Tree tree;
		std::vector<Tree::ItemNode const *> itemNodes;
		State(uint32_t q) : tree(SignatureTraits(q)) {}
	};
	struct CreationState {
		///in ghId order!
		std::vector<Signature> regionSignatures;
		std::vector<Signature> cellSignatures;
		sserialize::SimpleBitVector processedItems;
		SignatureTraits::Combine combine;
	public:
		CreationState(SignatureTraits const & straits) : combine(straits.combine()) {}
	};
public:
	OPQGramsRTree(std::shared_ptr<liboscar::Static::OsmCompleter> cmp, uint32_t q) :
	cmp(cmp), state(q), cstate(state.tree.straits()) {}
public:
	void init();
	void create();
public:
	void serialize(sserialize::UByteArrayAdapter & treeData, sserialize::UByteArrayAdapter & traitsData);
public:
	void test();
	Signature cellSignature(uint32_t cellId);
	Signature itemSignature(uint32_t itemId);
private:
	std::string normalize(std::string const & str);
	sserialize::ItemIndex matchingItems(typename Tree::GeometryMatchPredicate & gmp, typename Tree::SignatureMatchPredicate & smp);
public:
	std::shared_ptr<liboscar::Static::OsmCompleter> cmp;
	State state;
	CreationState cstate;
	bool check{false};
	
};
