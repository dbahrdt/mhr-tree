#pragma once

#include <sserialize/strings/unicode_case_functions.h>

#include <liboscar/StaticOsmCompleter.h>
#include <liboscar/KVStats.h>

#include <srtree/SRTree.h>

template<typename T_QGRAM_TRAITS = srtree::detail::PQGramTraits>
struct OPQGramsRTree {
	using Tree = srtree::SRTree<
	T_QGRAM_TRAITS,
	srtree::detail::GeoRectGeometryTraits,
	12,
	32>;
	using SignatureTraits = typename Tree::SignatureTraits;
	using Signature = typename Tree::Signature;
	struct State {
		Tree tree;
		std::vector<typename Tree::ItemNode const *> itemNodes;
		State(uint32_t q) : tree(SignatureTraits(q)) {}
	};
	struct CreationState {
		///in ghId order!
		std::vector<Signature> regionSignatures;
		std::vector<Signature> cellSignatures;
		sserialize::SimpleBitVector processedItems;
		typename SignatureTraits::Combine combine;
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

//Implementation
template<typename T_QGRAM_TRAITS>
void
OPQGramsRTree<T_QGRAM_TRAITS>::create() {
	sserialize::ProgressInfo pinfo;
	
	pinfo.begin(cmp->store().size(), "Gathering candidate strings");
	for(uint32_t i(0), s(cmp->store().size()); i < s; ++i) {
		auto item = cmp->store().kvItem(i);
		for(uint32_t j(0), js(item.size()); j < js; ++j) {
			std::string token = "@" + item.key(j) + ":" + item.value(j);
			state.tree.straits().add( normalize(token) );
		}
		pinfo(i);
	}
	pinfo.end();
	
	pinfo.begin(cmp->store().geoHierarchy().regionSize(), "Computing region signatures");
	for(uint32_t regionId(0), rs(cmp->store().geoHierarchy().regionSize()); regionId < rs; ++regionId) {
		cstate.regionSignatures.push_back(itemSignature(cmp->store().geoHierarchy().ghIdToStoreId(regionId)));
		pinfo(regionId);
	}
	pinfo.end();
	
	pinfo.begin(cmp->store().geoHierarchy().cellSize(), "Computing cell signatures");
	for(uint32_t cellId(0), cs(cmp->store().geoHierarchy().cellSize()); cellId < cs; ++cellId) {
		cstate.cellSignatures.push_back(cellSignature(cellId));
		pinfo(cellId);
	}
	pinfo.end();
	
	state.itemNodes.resize(cmp->store().size(), 0);
	
	uint32_t numProcItems = 0;
	
	pinfo.begin(cmp->store().size(), "Inserting items");
	uint32_t cs(cmp->store().geoHierarchy().cellSize());
	
	#pragma omp parallel for schedule(dynamic, 1)
	for(uint32_t cellId = 0; cellId < cs; ++cellId) {
		sserialize::ItemIndex cellItems = cmp->indexStore().at(cmp->store().geoHierarchy().cellItemsPtr(cellId));
		for(uint32_t itemId : cellItems) {
			bool itemProcessed = false;
			#pragma omp critical(processedItems)
			{
				itemProcessed = cstate.processedItems.isSet(itemId);
			}
			if (itemProcessed) {
				continue;
			}
			#pragma omp critical(processedItems)
			{
				cstate.processedItems.set(itemId);
			}
			auto b = cmp->store().geoShape(itemId).boundary();
			auto isig = itemSignature(itemId);
			for(auto x : cmp->store().cells(itemId)) {
				isig = cstate.combine(isig, cstate.cellSignatures.at(x));
			}
			#pragma omp critical(treeAccess)
			{
				state.itemNodes.at(itemId) = state.tree.insert(b, isig, itemId);
			}
			
			#pragma omp atomic
			++numProcItems;
			
			#pragma omp critical(pinfo)
			{
				pinfo(numProcItems);
			}
		}
		if (check) {
			#pragma omp critical(treeAccess)
			if (!state.tree.checkConsistency()) {
				throw sserialize::CreationException("Tree failed consistency check!");
			}
		}
	}
	pinfo.end();
	
	if (check && !state.tree.checkConsistency()) {
		throw sserialize::CreationException("Tree failed consistency check!");
	}
	
	pinfo.begin(1, "Calculating signatures");
	state.tree.recalculateSignatures();
	pinfo.end();
}

template<typename T_QGRAM_TRAITS>
NO_OPTIMIZE
void
OPQGramsRTree<T_QGRAM_TRAITS>::test() {
	if (!state.tree.checkConsistency()) {
		std::cerr << "Tree is not consistent" << std::endl;
		return;
	}
	
	//check if tree returns all elements of each cell
	sserialize::ProgressInfo pinfo;
	auto const & gh = cmp->store().geoHierarchy();
	pinfo.begin(gh.cellSize(), "Testing spatial constraint");
	for(uint32_t cellId(0), cs(gh.cellSize()); cellId < cs; ++cellId) {
		auto cellItems = cmp->indexStore().at(gh.cellItemsPtr(cellId));
		auto cb = gh.cellBoundary(cellId);
		
		std::vector<uint32_t> tmp;
		state.tree.find(state.tree.gtraits().mayHaveMatch(cb), std::back_inserter(tmp));
		std::sort(tmp.begin(), tmp.end());
		sserialize::ItemIndex result(std::move(tmp));
		
		if ( (cellItems - result).size() ) {
			std::cout << "Incorrect result for cell " << cellId << std::endl;
		}
	}
	pinfo.end();
	//now check the most frequent key:value pair combinations
	std::cout << "Computing store kv stats..." << std::flush;
	auto kvstats = liboscar::KVStats(cmp->store()).stats(sserialize::ItemIndex(sserialize::RangeGenerator<uint32_t>(0, cmp->store().size())), 0);
	std::cout << "done" << std::endl;
	auto topkv = kvstats.topkv(100, [](auto const & a, auto const & b) {
		return a.valueCount < b.valueCount;
	});
	std::vector<std::string> kvstrings;
	for(auto const & x : topkv) {
		std::string str = "@";
		str += cmp->store().keyStringTable().at(x.keyId);
		str += ":";
		str += cmp->store().valueStringTable().at(x.valueId);
		kvstrings.push_back( normalize(str) );
	}
	
	uint32_t failedQueries = 0;
	
	auto storeBoundary = cmp->store().boundary();
	pinfo.begin(kvstrings.size(), "Testing string constraint");
	for(std::size_t i(0), s(kvstrings.size()); i < s; ++i) {
		std::string const & str = kvstrings[i];
		sserialize::ItemIndex items = cmp->cqrComplete("\"" + str + "\"").flaten();
		
		auto smp = state.tree.straits().mayHaveMatch(str, 0);
		auto gmp = state.tree.gtraits().mayHaveMatch(storeBoundary);
		
		std::vector<uint32_t> tmp;
		state.tree.find(gmp, smp, std::back_inserter(tmp));
		std::sort(tmp.begin(), tmp.end());
		sserialize::ItemIndex result(std::move(tmp));
		
		if ( (items - result).size() ) {
			std::cout << "Incorrect result for query string " << str << ": " << (items - result).size() << std::endl;
			++failedQueries;

			sserialize::ItemIndex mustResult(matchingItems(gmp, smp));
			if (result != mustResult) {
				std::cout << "Tree does not return all valid items: "<< std::endl;
				std::cout << "Correct result: " << mustResult.size() << std::endl;
				std::cout << "Have result: " << result.size() << std::endl;
				std::cout << "Missing: " << (mustResult - result).size() << std::endl;
				std::cout << "Invalid: " << (result - mustResult).size() << std::endl;
			}
		}
		pinfo(i);
	}
	pinfo.end();
	
	failedQueries = 0;
	pinfo.begin(kvstrings.size(), "Testing string+boundary constraint");
	for(std::size_t i(0), s(kvstrings.size()); i < s; ++i) {
		std::string const & str = kvstrings[i];
		
		auto smp = state.tree.straits().mayHaveMatch(str, 0);
		auto cqr = cmp->cqrComplete("\"" + str + "\"");
		for(uint32_t i(0), s(cqr.cellCount()); i < s; ++i) {
			std::vector<uint32_t> tmp;
			auto gmp = state.tree.gtraits().mayHaveMatch(cmp->store().geoHierarchy().cellBoundary(cqr.cellId(i)));
			state.tree.find(gmp, smp, std::back_inserter(tmp));
			std::sort(tmp.begin(), tmp.end());
			sserialize::ItemIndex result(std::move(tmp));
			
			if ( (cqr.items(i) - result).size() ) {
				std::cout << "Incorrect result for query string " << str << " and cell " << cqr.cellId(i) << std::endl;
				++failedQueries;
				
				sserialize::ItemIndex mustResult(matchingItems(gmp, smp));
				if (result != mustResult) {
					std::cout << "Tree does not return all valid items: "<< std::endl;
					std::cout << "Correct result: " << mustResult.size() << std::endl;
					std::cout << "Have result: " << result.size() << std::endl;
					std::cout << "Missing: " << (mustResult - result).size() << std::endl;
					std::cout << "Invalid: " << (result - mustResult).size() << std::endl;
				}
			}
		}
		
		pinfo(i);
	}
	pinfo.end();
	
	if (failedQueries) {
		std::cout << "There were " << failedQueries << " failed queries out of " << kvstrings.size() << std::endl;
	}
}


template<typename T_QGRAM_TRAITS>
void
OPQGramsRTree<T_QGRAM_TRAITS>::serialize(sserialize::UByteArrayAdapter & treeData, sserialize::UByteArrayAdapter & traitsData) {
	state.tree.serialize(treeData);
	traitsData << state.tree.straits() << state.tree.gtraits();
}

template<typename T_QGRAM_TRAITS>
typename OPQGramsRTree<T_QGRAM_TRAITS>::Signature
OPQGramsRTree<T_QGRAM_TRAITS>::cellSignature(uint32_t cellId) {
	auto const & gh = cmp->store().geoHierarchy();
	auto cell = gh.cell(cellId);
	Signature sig;
	for(uint32_t i(0), s(cell.parentsSize()); i < s; ++i) {
		sig = cstate.combine(sig, cstate.regionSignatures.at( cell.parent(i) ) );
	}
	return sig;
}

template<typename T_QGRAM_TRAITS>
typename OPQGramsRTree<T_QGRAM_TRAITS>::Signature
OPQGramsRTree<T_QGRAM_TRAITS>::itemSignature(uint32_t itemId) {
	auto item = cmp->store().at(itemId);
	Signature sig;
	for(uint32_t i(0), s(item.size()); i < s; ++i) {
		std::string token = "@" + item.key(i) + ":" + item.value(i);
		auto tsig = state.tree.straits().signature(normalize(token));
		sig = cstate.combine(sig, tsig);
	}
	return sig;
}

template<typename T_QGRAM_TRAITS>
sserialize::ItemIndex
OPQGramsRTree<T_QGRAM_TRAITS>::matchingItems(typename Tree::GeometryMatchPredicate & gmp, typename Tree::SignatureMatchPredicate & smp) {
	std::vector<uint32_t> validItems;
	//check all items directly
	for(uint32_t i(cmp->store().geoHierarchy().regionSize()), s(cmp->store().size()); i < s; ++i) {
		auto x = state.itemNodes.at(i);
		SSERIALIZE_CHEAP_ASSERT(x);
		if (gmp(x->boundary()) && smp( x->payload() ) ) {
			validItems.push_back(x->item());
		}
		SSERIALIZE_CHEAP_ASSERT_EQUAL(i, x->item());
	}
	return sserialize::ItemIndex(std::move(validItems));
}

template<typename T_QGRAM_TRAITS>
std::string
OPQGramsRTree<T_QGRAM_TRAITS>::normalize(std::string const & str) {
	return sserialize::unicode_to_lower(str);
}
