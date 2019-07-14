#pragma once

#include <srtree/SRTree.h>

#include <srtree/QGram.h>

#include <sserialize/strings/unicode_case_functions.h>
#include <sserialize/mt/ThreadPool.h>
#include <liboscar/StaticOsmCompleter.h>
#include <liboscar/KVStats.h>
#include <sserialize/utility/debug.h>
#include <sserialize/utility/debuggerfunctions.h>
#include <sserialize/utility/assert.h>

template<typename T_SIGNATURE_TRAITS>
struct OMHRTree {
	using SignatureTraits = T_SIGNATURE_TRAITS;
	using GeometryTraits = srtree::detail::GeoRectGeometryTraits;
	using Tree = srtree::SRTree<
		SignatureTraits,
		GeometryTraits,
		12,
		32
	>;
	using Signature = typename Tree::Signature;
	struct State {
		Tree tree;
		std::mutex treeLock;
		std::vector<typename Tree::ItemNode const *> itemNodes;
		State(std::size_t q, std::size_t hashSize) : tree(SignatureTraits(q, hashSize)) {}
	};
	struct CreationState {
		///in ghId order!
		std::vector<Signature> regionSignatures;
		std::vector<Signature> cellSignatures;
		sserialize::SimpleBitVector processedItems;
		std::mutex processedItemsLock;
		typename SignatureTraits::Combine combine;
	public:
		CreationState(SignatureTraits const & straits) : combine(straits.combine()) {}
	};
public:
	OMHRTree(std::shared_ptr<liboscar::Static::OsmCompleter> cmp, std::size_t q, std::size_t hashSize) :
	cmp(cmp), state(q, hashSize), cstate(state.tree.straits()) {}
public:
	void setCheck(bool check) { this->check = check; }
public:
	void init();
	void create(uint32_t numThreads);
public:
	void serialize(sserialize::UByteArrayAdapter & treeData, sserialize::UByteArrayAdapter & traitsData);
	bool equal(sserialize::UByteArrayAdapter treeData, sserialize::UByteArrayAdapter traitsData);
public:
	void test();
	Signature cellSignature(uint32_t cellId);
	Signature itemSignature(uint32_t itemId);
private:
	std::string normalize(std::string const & str) const;
	sserialize::ItemIndex matchingItems(typename Tree::GeometryMatchPredicate & gmp, typename Tree::SignatureMatchPredicate & smp);
	std::set<std::string> strings(liboscar::Static::OsmKeyValueObjectStore::Item const & item, bool inherited) const;
	std::set< std::pair<std::string, uint32_t> > pqgrams(std::set<std::string> const & strs) const;
	std::set<std::string> qgrams(std::set<std::string> const & strs) const;
	inline std::string keyValue(liboscar::Static::OsmKeyValueObjectStore::Item const & item, uint32_t pos) const;
public:
	std::shared_ptr<liboscar::Static::OsmCompleter> cmp;
	State state;
	CreationState cstate;
	bool check{false};
	
};

template<typename T_PARAMETRISED_HASH_FUNCTION>
void
OMHRTree<T_PARAMETRISED_HASH_FUNCTION>::create(uint32_t numThreads) {
	sserialize::ProgressInfo pinfo;
	cstate.regionSignatures.resize(cmp->store().geoHierarchy().regionSize());
	pinfo.begin(cmp->store().geoHierarchy().regionSize(), "Computing region signatures");
	for(uint32_t regionId(0), rs(cmp->store().geoHierarchy().regionSize()); regionId < rs; ++regionId) {
		cstate.regionSignatures.at(regionId) = itemSignature(cmp->store().geoHierarchy().ghIdToStoreId(regionId));
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
	
	std::atomic<uint32_t> numProcItems = 0;
	
	pinfo.begin(cmp->store().size(), "Inserting items");
	std::atomic<uint32_t> cellIdIt{0};
	sserialize::ThreadPool::execute([&](){
		uint32_t cs(cmp->store().geoHierarchy().cellSize());
		while (true) {
			uint32_t cellId = cellIdIt.fetch_add(1, std::memory_order_relaxed);
			if (cellId >= cs) {
				break;
			}
			sserialize::ItemIndex cellItems = cmp->indexStore().at(cmp->store().geoHierarchy().cellItemsPtr(cellId));
			for(uint32_t itemId : cellItems) {
				if (cstate.processedItems.isSet(itemId)) {
					continue;
				}
				{
					std::lock_guard<std::mutex> lck(cstate.processedItemsLock);
					cstate.processedItems.set(itemId);
				}
				auto b = cmp->store().geoShape(itemId).boundary();
				auto isig = itemSignature(itemId);
				for(auto x : cmp->store().cells(itemId)) {
					isig = cstate.combine(isig, cstate.cellSignatures.at(x));
				}
				SSERIALIZE_EXPENSIVE_ASSERT_EXEC(auto istrs = strings(cmp->store().at(itemId), true));
				SSERIALIZE_EXPENSIVE_ASSERT_EQUAL(isig, state.tree.straits().signature(istrs.begin(), istrs.end()));
				{
					std::lock_guard<std::mutex> lck(state.treeLock);
					state.itemNodes.at(itemId) = state.tree.insert(b, isig, itemId);
				}
				++numProcItems;
				pinfo(numProcItems);
			}
			if (check) {
				std::lock_guard<std::mutex> lck(state.treeLock);
				if (!state.tree.checkConsistency()) {
					throw sserialize::CreationException("Tree failed consistency check!");
				}
			}
		}
	},
	numThreads,
	sserialize::ThreadPool::CopyTaskTag());
	pinfo.end();
	
	if (check && !state.tree.checkConsistency()) {
		throw sserialize::CreationException("Tree failed consistency check!");
	}
	
	pinfo.begin(1, "Calculating signatures");
	state.tree.recalculateSignatures();
	pinfo.end();
}


template<typename T_PARAMETRISED_HASH_FUNCTION>
void
OMHRTree<T_PARAMETRISED_HASH_FUNCTION>::serialize(sserialize::UByteArrayAdapter & treeData, sserialize::UByteArrayAdapter & traitsData) {
	state.tree.serialize(treeData);
	traitsData << state.tree.straits() << state.tree.gtraits();
	if (check && !equal(treeData, traitsData)) {
		throw sserialize::CreationException("Serialized tree is not equal to in-memory structure");
	}
}


template<typename T_PARAMETRISED_HASH_FUNCTION>
bool
OMHRTree<T_PARAMETRISED_HASH_FUNCTION>::equal(sserialize::UByteArrayAdapter treeData, sserialize::UByteArrayAdapter traitsData) {
	using StaticTree = srtree::Static::SRTree<typename SignatureTraits::StaticTraits, typename GeometryTraits::StaticTraits>;
	typename SignatureTraits::StaticTraits sstraits;
	typename GeometryTraits::StaticTraits sgtraits;
	traitsData >> sstraits >> sgtraits;
	StaticTree stree(treeData, std::move(sstraits), std::move(sgtraits));
	return state.tree.checkEquality(stree);
}

template<typename T_PARAMETRISED_HASH_FUNCTION>
void
OMHRTree<T_PARAMETRISED_HASH_FUNCTION>::test() {
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
	uint32_t totalQueries = 0;
	
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
			std::cout << "Incorrect result for query string " << str << ": " << (items - result).size() << '/' << items.size() << std::endl;
			++failedQueries;
			sserialize::ItemIndex diff = items - result;
		
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
	totalQueries = 0;
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
			++totalQueries;
		}
		
		pinfo(i);
	}
	pinfo.end();
	
	if (failedQueries) {
		std::cout << "There were " << failedQueries << " failed queries out of " << totalQueries << std::endl;
	}
}

template<typename T_PARAMETRISED_HASH_FUNCTION>
typename OMHRTree<T_PARAMETRISED_HASH_FUNCTION>::Signature
OMHRTree<T_PARAMETRISED_HASH_FUNCTION>::cellSignature(uint32_t cellId) {
	auto const & gh = cmp->store().geoHierarchy();
	auto cell = gh.cell(cellId);
	Signature sig;
	for(uint32_t i(0), s(cell.parentsSize()); i < s; ++i) {
		sig = cstate.combine(sig, cstate.regionSignatures.at( cell.parent(i) ) );
	}
	return sig;
}

template<typename T_PARAMETRISED_HASH_FUNCTION>
typename OMHRTree<T_PARAMETRISED_HASH_FUNCTION>::Signature
OMHRTree<T_PARAMETRISED_HASH_FUNCTION>::itemSignature(uint32_t itemId) {
	auto item = cmp->store().at(itemId);
	Signature sig;
	for(uint32_t i(0), s(item.size()); i < s; ++i) {
		sig = cstate.combine(sig, state.tree.straits().signature(keyValue(item, i)));
	}
	return sig;
}

template<typename T_PARAMETRISED_HASH_FUNCTION>
NO_OPTIMIZE
sserialize::ItemIndex
OMHRTree<T_PARAMETRISED_HASH_FUNCTION>::matchingItems(typename Tree::GeometryMatchPredicate & gmp, typename Tree::SignatureMatchPredicate & smp) {
	std::vector<uint32_t> validItems;
	//check all items directly
	for(uint32_t i(cmp->store().geoHierarchy().regionSize()), s(cmp->store().size()); i < s; ++i) {
		auto x = state.itemNodes.at(i);
		SSERIALIZE_CHEAP_ASSERT(x);
		auto ib = x->boundary();
		auto ip = x->payload();
		if (gmp(ib) && smp( ip ) ) {
			validItems.push_back(x->item());
		}
		SSERIALIZE_CHEAP_ASSERT_EQUAL(i, x->item());
	}
	return sserialize::ItemIndex(std::move(validItems));
}

template<typename T_PARAMETRISED_HASH_FUNCTION>
std::set<std::string>
OMHRTree<T_PARAMETRISED_HASH_FUNCTION>::strings(liboscar::Static::OsmKeyValueObjectStore::Item const & item, bool inherited) const {
	std::set<std::string> result;

	auto f = [&result, this](liboscar::Static::OsmKeyValueObjectStore::Item const & item) {
		for(uint32_t i(0), s(item.size()); i < s; ++i) {
			result.insert(keyValue(item, i));
		}
	};
	
	f(item);
	
	if (inherited) {
		std::set<uint32_t> regionIds;
		auto cells = item.cells();
		for(uint32_t cellId : cells) {
			auto cell = cmp->store().geoHierarchy().cell(cellId);
			for(uint32_t i(0), s(cell.parentsSize()); i < s; ++i) {
				regionIds.insert(
					cmp->store().geoHierarchy().ghIdToStoreId(
						cell.parent(i)
					)
				);
			}
		}
		for(uint32_t regionId : regionIds) {
			f( cmp->store().at(regionId) );
		}
	}
	return result;
}

template<typename T_PARAMETRISED_HASH_FUNCTION>
std::set<std::string>
OMHRTree<T_PARAMETRISED_HASH_FUNCTION>::qgrams(std::set<std::string> const & strs) const {
	std::set<std::string> result;
	for(std::string const & x : strs) {
		srtree::QGram qg(x, state.tree.straits().q());
		for(uint32_t j(0), js(qg.size()); j < js; ++j) {
			result.emplace(qg.at(j));
		}
	};
	return result;
}

template<typename T_PARAMETRISED_HASH_FUNCTION>
std::set< std::pair<std::string, uint32_t> >
OMHRTree<T_PARAMETRISED_HASH_FUNCTION>::pqgrams(std::set<std::string> const & strs) const {
	std::set< std::pair<std::string, uint32_t> > result;
	for(std::string const & x : strs) {
		srtree::QGram qg(x, state.tree.straits().q());
		for(uint32_t j(0), js(qg.size()); j < js; ++j) {
			result.emplace(qg.at(j), j);
		}
	};
	return result;
}

template<typename T_PARAMETRISED_HASH_FUNCTION>
std::string
OMHRTree<T_PARAMETRISED_HASH_FUNCTION>::keyValue(liboscar::Static::OsmKeyValueObjectStore::Item const & item, uint32_t pos) const {
	return normalize("@" + item.key(pos) + ":" + item.value(pos));
}

template<typename T_PARAMETRISED_HASH_FUNCTION>
std::string
OMHRTree<T_PARAMETRISED_HASH_FUNCTION>::normalize(std::string const & str) const {
	return sserialize::unicode_to_lower(str);
}
