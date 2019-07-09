#include <srtree/Static/SRTree.h>
#include <srtree/PQGramTraits.h>
#include <srtree/Static/DedupDeserializationTraitsAdapter.h>
#include <srtree/Static/StringSetTraits.h>
#include <liboscar/AdvancedOpTree.h>
#include <liboscar/StaticOsmCompleter.h>
#include <liboscar/KVStats.h>
#include <sserialize/containers/ItemIndex.h>
#include <sserialize/strings/unicode_case_functions.h>

enum TreeType {
	TT_INVALID,
	TT_MINWISE_LCG_32,
	TT_MINWISE_LCG_64,
	TT_MINWISE_SHA,
	TT_MINWISE_LCG_32_DEDUP,
	TT_MINWISE_LCG_64_DEDUP,
	TT_MINWISE_SHA_DEDUP,
	TT_STRINGSET,
	TT_QGRAM,
	TT_QGRAM_DEDUP
};

struct Config {
	std::string indir;
	std::string oscarDir;
	bool test{false};
	TreeType tt;
	std::vector<std::string> queries;
};

struct Data {
	sserialize::UByteArrayAdapter treeData;
	sserialize::UByteArrayAdapter traitsData;
	liboscar::Static::OsmCompleter cmp;
};

template<typename T_SIGNATURE_TRAITS>
struct Completer {
	static constexpr uint32_t SignatureSize = 56;
	using Tree = srtree::Static::SRTree<
		T_SIGNATURE_TRAITS,
		srtree::detail::GeoRectGeometryTraits
	>;
	using SignatureTraits = typename Tree::SignatureTraits;
	using GeometryTraits = typename Tree::GeometryTraits;
	
	Completer(sserialize::UByteArrayAdapter treeData, sserialize::UByteArrayAdapter traitsData) {
		SignatureTraits straits;
		GeometryTraits gtraits;
		traitsData >> straits >> gtraits;
		tree = std::move( Tree(treeData, std::move(straits), std::move(gtraits) ) );
	}
	sserialize::ItemIndex complete(liboscar::AdvancedOpTree const & tree);
	
	sserialize::ItemIndex complete(std::string const & str) {
		liboscar::AdvancedOpTree optree;
		optree.parse(str);
		if (!optree.root()) {
			return sserialize::ItemIndex();
		}
		struct Transformer {
			using SMP = typename SignatureTraits::MayHaveMatch;
			using GMP = typename GeometryTraits::MayHaveMatch;
			void operator()(liboscar::AdvancedOpTree::Node const & node) {
				using OpType = liboscar::AdvancedOpTree::Node::OpType;
				switch (node.baseType) {
					default:
						break;
				}
			}
		};
		Transformer t;
		t( *(optree.root()) );
		std::vector<uint32_t> result;
// 		tree.find(t.smp, t.gmp, std::back_inserter(result));
		std::sort(result.begin(), result.end());
		return sserialize::ItemIndex(std::move(result));
	}
	
	void complete(std::vector<std::string> const & strs) {
		for(auto x : strs) {
			sserialize::ItemIndex result = complete(x);
			std::cout << x << ": " << result.size() << " items";
		}
	}
	
	std::string normalize(std::string const & str) const {
		return sserialize::unicode_to_lower(str);
	}
	
	sserialize::ItemIndex matchingItems(typename GeometryTraits::MayHaveMatch gmp, typename SignatureTraits::MayHaveMatch smp) {
		struct Recurser {
			std::vector<uint32_t> result;
			typename GeometryTraits::MayHaveMatch & gmp;
			typename SignatureTraits::MayHaveMatch & smp;
			void operator()(typename Tree::MetaNode const & node) {
				if (node.type() == node.ITEM) {
					if (gmp(node.boundary()) && smp(node.signature())) {
						result.push_back(node.item());
					}
				}
				else {
					for(uint32_t i(0), s(node.numberOfChildren()); i < s; ++i) {
						(*this)(node.child(i));
					}
				}
			}
			Recurser(typename GeometryTraits::MayHaveMatch & gmp, typename SignatureTraits::MayHaveMatch & smp) :
			gmp(gmp),
			smp(smp)
			{}
		};
		Recurser rec(gmp, smp);
		rec(tree.root());
		return sserialize::ItemIndex(std::move(rec.result));
	}
	
	void test(liboscar::Static::OsmCompleter & cmp) {
		//check if tree returns all elements of each cell
		sserialize::ProgressInfo pinfo;
		auto const & gh = cmp.store().geoHierarchy();
		pinfo.begin(gh.cellSize(), "Testing spatial constraint");
		for(uint32_t cellId(0), cs(gh.cellSize()); cellId < cs; ++cellId) {
			auto cellItems = cmp.indexStore().at(gh.cellItemsPtr(cellId));
			auto cb = gh.cellBoundary(cellId);
			
			std::vector<uint32_t> tmp;
			tree.find(tree.gtraits().mayHaveMatch(cb), std::back_inserter(tmp));
			std::sort(tmp.begin(), tmp.end());
			sserialize::ItemIndex result(std::move(tmp));
			
			if ( (cellItems - result).size() ) {
				std::cout << "Incorrect result for cell " << cellId << std::endl;
			}
		}
		pinfo.end();
		//now check the most frequent key:value pair combinations
		std::cout << "Computing store kv stats..." << std::flush;
		auto kvstats = liboscar::KVStats(cmp.store()).stats(sserialize::ItemIndex(sserialize::RangeGenerator<uint32_t>(0, cmp.store().size())), 0);
		std::cout << "done" << std::endl;
		auto topkv = kvstats.topkv(100, [](auto const & a, auto const & b) {
			return a.valueCount < b.valueCount;
		});
		std::vector<std::string> kvstrings;
		for(auto const & x : topkv) {
			std::string str = "@";
			str += cmp.store().keyStringTable().at(x.keyId);
			str += ":";
			str += cmp.store().valueStringTable().at(x.valueId);
			kvstrings.push_back( normalize(str) );
		}
		
		uint32_t failedQueries = 0;
		uint32_t totalQueries = 0;
		
		auto storeBoundary = cmp.store().boundary();
		pinfo.begin(kvstrings.size(), "Testing string constraint");
		for(std::size_t i(0), s(kvstrings.size()); i < s; ++i) {
			std::string const & str = kvstrings[i];
			sserialize::ItemIndex items = cmp.cqrComplete("\"" + str + "\"").flaten();
			
			auto smp = tree.straits().mayHaveMatch(str, 0);
			auto gmp = tree.gtraits().mayHaveMatch(storeBoundary);
			
			std::vector<uint32_t> tmp;
			tree.find(gmp, smp, std::back_inserter(tmp));
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
				#ifdef SSERIALIZE_EXPENSIVE_ASSERT_ENABLED
				else {
					srtree::QGram strQ(str, state.tree.straits().q());
					auto strSg = state.tree.straits().signature(str);
					for(uint32_t itemId : diff) {
						auto ip = state.itemNodes.at(itemId)->payload();
						if (!smp(ip)) {
							auto item = cmp->store().at(itemId);
							auto strs = this->strings(item, true);
							auto pqgrams = this->pqgrams(strs);
							auto qgrams = this->qgrams(strs);
							std::cout << "Missing item has " << strs.size() << " different strings" << std::endl;
							std::cout << "Missing item has " << pqgrams.size() << " different pq-grams" << std::endl;
							uint32_t commonQGrams = 0;
							for(uint32_t i(0), s(strQ.size()); i < s; ++i) {
								if (pqgrams.count(std::pair<std::string, uint32_t>(strQ.at(i), i))) {
									commonQGrams += 1;
								}
							}						
							auto mip = state.tree.straits().signature(qgrams.begin(), qgrams.end());
							if (ip != mip) {
								std::cout << "ERROR: Item signature in tree differs from manually computed one and manually computed signature would";
								if (smp(mip)) {
									std::cout << " ";
								}
								else{
									std::cout << " NOT";
								}
								std::cout << " match";
							}
							
							std::cout << "Missing info:" << std::endl;
							std::cout << "Matching strings: " << strs.count(str) << "" << std::endl;
							std::cout << "Common qgrams: " << commonQGrams << "/" << strQ.size() << std::endl;
							std::cout << "Common signature entries: " << (strSg / ip) << std::endl;
							std::cout << "Approximated common qgrams" << smp.intersectionSize(ip) << std::endl;
							std::cout << "Id: " << itemId << std::endl;
						}
					}
				}
				#endif
				
			}
			pinfo(i);
		}
		pinfo.end();
		
		failedQueries = 0;
		totalQueries = 0;
		pinfo.begin(kvstrings.size(), "Testing string+boundary constraint");
		for(std::size_t i(0), s(kvstrings.size()); i < s; ++i) {
			std::string const & str = kvstrings[i];
			
			auto smp = tree.straits().mayHaveMatch(str, 0);
			auto cqr = cmp.cqrComplete("\"" + str + "\"");
			for(uint32_t i(0), s(cqr.cellCount()); i < s; ++i) {
				std::vector<uint32_t> tmp;
				auto gmp = tree.gtraits().mayHaveMatch(cmp.store().geoHierarchy().cellBoundary(cqr.cellId(i)));
				tree.find(gmp, smp, std::back_inserter(tmp));
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
	Tree tree;
};

void help() {
	std::cout << "prg -i <input dir> -o <oscar dir> -t <minwise-lcg|minwise-sha|stringset|qgram> -m <query> --test" << std::endl;
}

int main(int argc, char ** argv) {
	Config cfg;
	Data data;
	for(int i(1); i < argc; ++i) {
		std::string token(argv[i+1]);
		if (token == "-i" && i+1 < argc) {
			cfg.indir = std::string(argv[i+1]);
			++i;
		}
		else if (token == "-o" && i+1 < argc) {
			cfg.oscarDir = std::string(argv[i+1]);
			++i;
		}
		else if ("-t" == token && i+1 < argc) {
			token = std::string(argv[i+1]);
			if ("minwise-lcg32" == token) {
				cfg.tt = TT_MINWISE_LCG_32;
			}
			else if ("minwise-lcg64" == token) {
				cfg.tt = TT_MINWISE_LCG_64;
			}
			else if ("minwise-sha" == token) {
				cfg.tt = TT_MINWISE_SHA;
			}
			else if ("minwise-lcg32-dedup" == token) {
				cfg.tt = TT_MINWISE_LCG_32_DEDUP;
			}
			else if ("minwise-lcg64-dedup" == token) {
				cfg.tt = TT_MINWISE_LCG_64_DEDUP;
			}
			else if ("minwise-sha-dedup" == token) {
				cfg.tt = TT_MINWISE_SHA_DEDUP;
			}
			else if ("stringset" == token) {
				cfg.tt = TT_STRINGSET;
			}
			else if ("qgram" == token) {
				cfg.tt = TT_QGRAM;
			}
			else if ("qgram-dedup" == token) {
				cfg.tt = TT_QGRAM_DEDUP;
			}
			else {
				help();
				std::cerr << "Invalid tree type: " << token << " at position " << i-1 << std::endl;
				return -1;
			}
			++i;
		}
		else if ("-m" == token) {
			cfg.queries.emplace_back( argv[i+1] );
			++i;
		}
		else if ("--test" == token) {
			cfg.test = true;
		}
	}
	
	data.treeData = sserialize::UByteArrayAdapter::openRo(cfg.indir + "/tree", false);
	data.traitsData = sserialize::UByteArrayAdapter::openRo(cfg.indir + "/traits", false);
	
	if (cfg.oscarDir.size()) {
		data.cmp.setAllFilesFromPrefix(cfg.oscarDir);
		data.cmp.energize();
	}
	
	switch(cfg.tt) {
	case TT_MINWISE_LCG_32:
	{
		constexpr std::size_t SignatureSize = 56;
		using Hash = srtree::detail::MinWisePermutation::LinearCongruentialHash<32>;
		using Traits = srtree::detail::MinWiseSignatureTraits<SignatureSize, Hash>;
		Completer<Traits> tcmp(data.treeData, data.traitsData);
		if (cfg.test) {
			tcmp.test(data.cmp);
		}
		tcmp.complete(cfg.queries);
	}
		break;
	case TT_MINWISE_LCG_64:
	{
		constexpr std::size_t SignatureSize = 56;
		using Hash = srtree::detail::MinWisePermutation::LinearCongruentialHash<64>;
		using Traits = srtree::detail::MinWiseSignatureTraits<SignatureSize, Hash>;
		Completer<Traits> tcmp(data.treeData, data.traitsData);
		if (cfg.test) {
			tcmp.test(data.cmp);
		}
		tcmp.complete(cfg.queries);
	}
		break;
	case TT_MINWISE_SHA:
	{
		constexpr std::size_t SignatureSize = 56;
		using Hash = srtree::detail::MinWisePermutation::CryptoPPHash<CryptoPP::SHA3_64>;
		using Traits = srtree::detail::MinWiseSignatureTraits<SignatureSize, Hash>;
		Completer<Traits> tcmp(data.treeData, data.traitsData);
		if (cfg.test) {
			tcmp.test(data.cmp);
		}
		tcmp.complete(cfg.queries);
	}
		break;
	case TT_MINWISE_LCG_32_DEDUP:
	{
		constexpr std::size_t SignatureSize = 56;
		using Hash = srtree::detail::MinWisePermutation::LinearCongruentialHash<32>;
		using BaseTraits = srtree::detail::MinWiseSignatureTraits<SignatureSize, Hash>;
		using Traits = srtree::detail::DedupDeserializationTraitsAdapter<BaseTraits>;
		Completer<Traits> tcmp(data.treeData, data.traitsData);
		if (cfg.test) {
			tcmp.test(data.cmp);
		}
		tcmp.complete(cfg.queries);
	}
		break;
	case TT_MINWISE_LCG_64_DEDUP:
	{
		constexpr std::size_t SignatureSize = 56;
		using Hash = srtree::detail::MinWisePermutation::LinearCongruentialHash<64>;
		using BaseTraits = srtree::detail::MinWiseSignatureTraits<SignatureSize, Hash>;
		using Traits = srtree::detail::DedupDeserializationTraitsAdapter<BaseTraits>;
		Completer<Traits> tcmp(data.treeData, data.traitsData);
		if (cfg.test) {
			tcmp.test(data.cmp);
		}
		tcmp.complete(cfg.queries);
	}
		break;
	case TT_MINWISE_SHA_DEDUP:
	{
		constexpr std::size_t SignatureSize = 56;
		using Hash = srtree::detail::MinWisePermutation::CryptoPPHash<CryptoPP::SHA3_64>;
		using BaseTraits = srtree::detail::MinWiseSignatureTraits<SignatureSize, Hash>;
		using Traits = srtree::detail::DedupDeserializationTraitsAdapter<BaseTraits>;
		Completer<Traits> tcmp(data.treeData, data.traitsData);
		if (cfg.test) {
			tcmp.test(data.cmp);
		}
		tcmp.complete(cfg.queries);
	}
		break;
	case TT_STRINGSET:
	{
		using Traits = srtree::Static::detail::StringSetTraits;
		Completer<Traits> tcmp(data.treeData, data.traitsData);
		if (cfg.test) {
			tcmp.test(data.cmp);
		}
		tcmp.complete(cfg.queries);
	}
		break;
	case TT_QGRAM:
	{
		using Traits = srtree::Static::detail::PQGramTraits;
		Completer<Traits> tcmp(data.treeData, data.traitsData);
		if (cfg.test) {
			tcmp.test(data.cmp);
		}
		tcmp.complete(cfg.queries);
	}
		break;
	case TT_QGRAM_DEDUP:
	{
		using BaseTraits = srtree::Static::detail::PQGramTraits;
		using Traits = srtree::detail::DedupDeserializationTraitsAdapter<BaseTraits>;
		Completer<Traits> tcmp(data.treeData, data.traitsData);
		if (cfg.test) {
			tcmp.test(data.cmp);
		}
		tcmp.complete(cfg.queries);
	}
		break;
	default:
		std::cerr << "Invalid tree type: " << cfg.tt << std::endl;
		return -1;
	}
	return 0;
}
