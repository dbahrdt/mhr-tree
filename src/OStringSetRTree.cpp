#include "OStringSetRTree.h"

#include <sserialize/utility/debuggerfunctions.h>
#include <sserialize/strings/unicode_case_functions.h>

#include <liboscar/KVStats.h>


void
OStringSetRTree::create() {
	sserialize::ProgressInfo pinfo;
	
	pinfo.begin(cmp->store().size(), "Gathering candidate strings");
	for(uint32_t i(0), s(cmp->store().size()); i < s; ++i) {
		auto item = cmp->store().kvItem(i);
		for(uint32_t j(0), js(item.size()); j < js; ++j) {
			std::string token = "@" + item.key(j) + ":" + item.value(j);
			state.str2Id.insert(normalize(token));
		}
		pinfo(i);
	}
	pinfo.end();
	//mark all strings as leafs
	for(auto it(state.str2Id.begin()), end(state.str2Id.end()); it != end; ++it) {
		it->second.value = StringId::GenericLeaf;
	}
	
	//this will add internal nodes
	state.str2Id.finalize();
	
	//mark all newly created nodes as internal, the other nodes get increasing ids
	{
		uint32_t i{0};
		for(auto it(state.str2Id.begin()), end(state.str2Id.end()); it != end; ++it, ++i) {
			if (!it->second.valid()) {
				it->second.value = StringId::Internal;
			}
			else {
				it->second.value = i;
			}
		}
	}
	
	pinfo.begin(cmp->store().geoHierarchy().regionSize(), "Computing region string ids");
	for(uint32_t regionId(0), rs(cmp->store().geoHierarchy().regionSize()); regionId < rs; ++regionId) {
		cstate.regionStrIds.push_back(
			itemStrIds(
				cmp->store().geoHierarchy().ghIdToStoreId(regionId)
			)
		);
		pinfo(regionId);
	}
	pinfo.end();
	
	pinfo.begin(cmp->store().geoHierarchy().cellSize(), "Computing cell string ids");
	for(uint32_t cellId(0), cs(cmp->store().geoHierarchy().cellSize()); cellId < cs; ++cellId) {
		cstate.cellStrIds.push_back( cellStrIds(cellId) );
		pinfo(cellId);
	}
	pinfo.end();
	
	state.itemNodes.resize(cmp->store().size(), 0);
	
	uint32_t numProcItems = 0;
	uint32_t cellCount(cmp->store().geoHierarchy().cellSize());
	pinfo.begin(cmp->store().size(), "Inserting items");
	#pragma omp parallel for schedule(dynamic, 1)
	for(uint32_t cellId = 0; cellId < cellCount; ++cellId) {
		sserialize::ItemIndex cellItems = cmp->indexStore().at(cmp->store().geoHierarchy().cellItemsPtr(cellId));
		for(uint32_t itemId : cellItems) {
			if (cstate.processedItems.isSet(itemId)) {
				continue;
			}
			cstate.processedItems.set(itemId);
			auto b = cmp->store().geoShape(itemId).boundary();
			auto iStrIds = itemStrIds(itemId);
			for(auto x : cmp->store().cells(itemId)) {
				iStrIds += cstate.cellStrIds.at(x);
			}
			auto isig = state.tree.straits().addSignature(iStrIds);
			#pragma omp critical
			state.itemNodes.at(itemId) = state.tree.insert(b, isig, itemId);
			SSERIALIZE_CHEAP_ASSERT_EQUAL(itemId, state.itemNodes.at(itemId)->item());
			#pragma omp atomic
			++numProcItems;
			pinfo(numProcItems);
		}
		if (check) {
			#pragma omp critical
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

// NO_OPTIMIZE
void
OStringSetRTree::test() {
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
		
		auto smp = state.tree.straits().mayHaveMatch(matchingStrings(str, false));
		auto gmp = state.tree.gtraits().mayHaveMatch(storeBoundary);
		
		std::vector<uint32_t> tmp;
		state.tree.find(gmp, smp, std::back_inserter(tmp));
		std::sort(tmp.begin(), tmp.end());
		sserialize::ItemIndex result(std::move(tmp));
		
		if ( (items - result).size() ) {
			std::cout << "Incorrect result for query string " << str << ": " << (items - result).size() << std::endl;
			++failedQueries;
			auto diff = items - result;
			if (diff.size() < 10) {
				for(auto itemId : diff) {
					std::cout << "Item " << itemId << " has the following associated strings:" << std::endl;
					auto itemStrIds = state.tree.straits().idxFactory().indexById( state.itemNodes.at(itemId)->payload() );
					for(auto strId : itemStrIds) {
						std::cout << state.str2Id.toStr((state.str2Id.begin()+strId)->first) << std::endl;
					}
					std::cout << std::endl;
				}
			}

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
		
		auto smp = state.tree.straits().mayHaveMatch(matchingStrings(str, false));
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


void
OStringSetRTree::serialize(sserialize::UByteArrayAdapter & treeData, sserialize::UByteArrayAdapter & traitsData) {
	state.tree.serialize(treeData);
	traitsData << state.tree.straits() << state.tree.gtraits();
	state.str2Id.append(traitsData);
}

sserialize::ItemIndex
OStringSetRTree::cellStrIds(uint32_t cellId) {
	auto const & gh = cmp->store().geoHierarchy();
	auto cell = gh.cell(cellId);
	std::vector<sserialize::ItemIndex> tmp;
	for(uint32_t i(0), s(cell.parentsSize()); i < s; ++i) {
		tmp.push_back(
			cstate.regionStrIds.at( cell.parent(i) )
		);
	}
	return sserialize::ItemIndex::unite(tmp);
}

sserialize::ItemIndex
OStringSetRTree::itemStrIds(uint32_t itemId) {
	auto item = cmp->store().kvItem(itemId);
	std::vector<uint32_t> tmp;
	for(uint32_t i(0), s(item.size()); i < s; ++i) {
		std::string token("@" + item.key(i) + ":" + item.value(i));
		tmp.push_back( state.str2Id.at( normalize(token) ).value );
	}
	std::sort(tmp.begin(), tmp.end());
	return sserialize::ItemIndex(std::move(tmp));
}

sserialize::ItemIndex
OStringSetRTree::matchingStrings(std::string const & str, bool prefixMatch) {
	if (!prefixMatch) {
		return sserialize::ItemIndex( std::vector<uint32_t>(1, state.str2Id.at(str).value) );
	}
	
	std::vector<uint32_t> tmp;
	auto node = state.str2Id.findNode(str.begin(), str.end(), true);
	for(auto it(node->rawBegin()), end(node->rawEnd()); it != end; ++it) {
		if (it->second.leaf()) {
			tmp.push_back(it->second.value);
		}
	}
	return sserialize::ItemIndex(std::move(tmp));
}

sserialize::ItemIndex
OStringSetRTree::matchingItems(typename Tree::GeometryMatchPredicate & gmp, typename Tree::SignatureMatchPredicate & smp) {
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

std::string
OStringSetRTree::normalize(std::string const & str) {
	return sserialize::unicode_to_lower(str);
}
