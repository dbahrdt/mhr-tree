#include <srtree/Static/SRTree.h>
#include <srtree/PQGramTraits.h>
#include <srtree/Static/DedupDeserializationTraitsAdapter.h>
#include <srtree/Static/StringSetTraits.h>
#include <liboscar/AdvancedOpTree.h>
#include <liboscar/StaticOsmCompleter.h>
#include <liboscar/KVStats.h>
#include <sserialize/containers/ItemIndex.h>
#include <sserialize/strings/unicode_case_functions.h>
#include <sserialize/search/StringCompleter.h>

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
	uint32_t bench{0};
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
	
	struct BenchEntry {
		std::vector<std::string> strs;
		std::vector<typename GeometryTraits::Boundary> bounds;
		void push_back(std::string const & str) {
			strs.push_back(str);
		}
		void push_back(typename GeometryTraits::Boundary const & b) {
			bounds.push_back(b);
		}
	};
	
	sserialize::ItemIndex complete(std::string const & str) {
		liboscar::AdvancedOpTree optree;
		optree.parse(str);
		if (!optree.root()) {
			return sserialize::ItemIndex();
		}
		struct Transformer {
			using SMP = typename SignatureTraits::MayHaveMatch;
// 			using GMP = typename GeometryTraits::MayHaveMatch;
			Completer * c;
			std::unique_ptr<SMP> operator()(liboscar::AdvancedOpTree::Node const & node) {
				using OpType = liboscar::AdvancedOpTree::Node::OpType;
				switch (node.subType) {
				case OpType::STRING:
				case OpType::STRING_ITEM:
				case OpType::STRING_REGION:
				{
					std::string q = node.value;
					sserialize::StringCompleter::normalize(q);
					return std::make_unique<SMP>(c->tree.straits().mayHaveMatch(q, 0));
				}
				case OpType::SET_OP:
				{
					auto first = (*this)(*node.children.front());
					auto second = (*this)(*node.children.back());
					switch(node.value.front()) {
					case ' ':
					case '/':
						return std::make_unique<SMP>( (*first) / (*second) );
					case '+':
						return std::make_unique<SMP>( (*first) + (*second) );
					default:
						throw sserialize::UnsupportedFeatureException("Op: " + node.value);
					};
				}
				default:
					throw sserialize::UnsupportedFeatureException("Op: " + node.subType);
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
	auto strs2OQ(std::vector<std::string> const & strs) {
		std::stringstream ss;
		for(std::string const & x : strs) {
			ss << '"' << x << '"' << ' ';
		}
		return ss.str();
	}
	
	auto bounds2OQ(std::vector<typename GeometryTraits::Boundary> const & bounds) {
		std::stringstream ss;
		std::copy(bounds.begin(), bounds.end(), std::ostream_iterator<typename GeometryTraits::Boundary>(ss, " + "));
		return ss.str();
	}
	
	auto strs2SMP(std::vector<std::string> const & strs) {
		auto it = strs.begin();
		auto smp = tree.straits().mayHaveMatch(*it, 0);
		for(++it; it != strs.end(); ++it) {
			smp = smp + tree.straits().mayHaveMatch(*it, 0);
		}
		return smp;
	}
	
	auto bounds2GMP(std::vector<typename GeometryTraits::Boundary> const & bounds) {
		auto it = bounds.begin();
		auto gmp = tree.gtraits().mayHaveMatch(*it);
		for(++it; it != bounds.end(); ++it) {
			gmp = gmp + tree.gtraits().mayHaveMatch(*it);
		}
		return gmp;
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
			pinfo(cellId);
		}
		pinfo.end();
		//now check the most frequent key:value pair combinations
		std::cout << "Computing store kv stats..." << std::flush;
		liboscar::KVStats kvStatsBuilder(cmp.store());
		auto kvstats = kvStatsBuilder.stats(sserialize::ItemIndex(sserialize::RangeGenerator<uint32_t>(0, cmp.store().size())), 0);
		std::cout << "done" << std::endl;
		auto topkv = kvstats.topkv(100, [](auto const & a, auto const & b) {
			return a.valueCount < b.valueCount;
		});
		
		std::vector< std::vector<std::string> > kvstrings;
		kvstrings.reserve(topkv.size());
		for(auto const & x : topkv) {
			std::string str = "@";
			str += cmp.store().keyStringTable().at(x.keyId);
			str += ":";
			str += cmp.store().valueStringTable().at(x.valueId);
			kvstrings.resize(kvstrings.size()+1);
			kvstrings.back().push_back( normalize(str) );
		}
		for(std::size_t i(0); i < kvstrings.size() && kvstrings.size() < 1000; ++i) {
			sserialize::ItemIndex items = cmp.cqrComplete(strs2OQ(kvstrings[i])).flaten();
			kvstats = kvStatsBuilder.stats(items);
			auto topkv = kvstats.topkv(5, [](auto const & a, auto const & b) {
				return a.valueCount < b.valueCount;
			});
			for(auto const & x : topkv) {
				std::string str = "@";
				str += cmp.store().keyStringTable().at(x.keyId);
				str += ":";
				str += cmp.store().valueStringTable().at(x.valueId);
				kvstrings.push_back(kvstrings[i]);
				kvstrings.back().push_back( normalize(str) );
			}
		}
		
		uint32_t failedQueries = 0;
		uint32_t totalQueries = 0;
		
		auto storeBoundary = cmp.store().boundary();
		pinfo.begin(kvstrings.size(), "Testing string constraint");
		for(std::size_t i(0), s(kvstrings.size()); i < s; ++i) {
			sserialize::ItemIndex items = cmp.cqrComplete(strs2OQ(kvstrings[i])).flaten();
			
			auto smp = strs2SMP(kvstrings[i]);
			auto gmp = tree.gtraits().mayHaveMatch(storeBoundary);
			
			std::vector<uint32_t> tmp;
			tree.find(gmp, smp, std::back_inserter(tmp));
			std::sort(tmp.begin(), tmp.end());
			sserialize::ItemIndex result(std::move(tmp));
			
			if ( (items - result).size() ) {
				using namespace sserialize;
				std::cout << "Incorrect result for query strings " << kvstrings[i] << ": " << (items - result).size() << '/' << items.size() << std::endl;
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
			
			auto smp = strs2SMP(kvstrings[i]);;
			auto cqr = cmp.cqrComplete(strs2OQ(kvstrings[i]));
			for(uint32_t i(0), s(cqr.cellCount()); i < s; ++i) {
				std::vector<uint32_t> tmp;
				auto gmp = tree.gtraits().mayHaveMatch(cmp.store().geoHierarchy().cellBoundary(cqr.cellId(i)));
				tree.find(gmp, smp, std::back_inserter(tmp));
				std::sort(tmp.begin(), tmp.end());
				sserialize::ItemIndex result(std::move(tmp));
				
				if ( (cqr.items(i) - result).size() ) {
					using namespace sserialize;
					std::cout << "Incorrect result for query strings " << kvstrings[i] << " and cell " << cqr.cellId(i) << std::endl;
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
	void bench(liboscar::Static::OsmCompleter & cmp, std::size_t numQueries) {
		sserialize::ProgressInfo pinfo;
		std::vector<BenchEntry> be;
		//now check the most frequent key:value pair combinations
		std::cout << "Computing store kv stats..." << std::flush;
		liboscar::KVStats kvStatsBuilder(cmp.store());
		auto kvstats = kvStatsBuilder.stats(sserialize::ItemIndex(sserialize::RangeGenerator<uint32_t>(0, cmp.store().size())), 0);
		std::cout << "done" << std::endl;
		auto topkv = kvstats.topkv(100, [](auto const & a, auto const & b) {
			return a.valueCount < b.valueCount;
		});
		
		for(auto const & x : topkv) {
			std::string str = "@";
			str += cmp.store().keyStringTable().at(x.keyId);
			str += ":";
			str += cmp.store().valueStringTable().at(x.valueId);
			be.resize(be.size()+1);
			be.back().push_back( normalize(str) );
		}
		pinfo.begin(numQueries, "Creating bench queries");
		for(std::size_t i(0); i < be.size() && be.size() < numQueries; ++i) {
			sserialize::ItemIndex items = cmp.cqrComplete(strs2OQ(be[i].strs)).flaten();
			kvstats = kvStatsBuilder.stats(items);
			auto topkv = kvstats.topkv(5, [](auto const & a, auto const & b) {
				return a.valueCount < b.valueCount;
			});
			for(auto const & x : topkv) {
				std::string str = "@";
				str += cmp.store().keyStringTable().at(x.keyId);
				str += ":";
				str += cmp.store().valueStringTable().at(x.valueId);
				be.push_back(be[i]);
				be.back().push_back( normalize(str) );
			}
			pinfo(i);
		}
		pinfo.end();
		//now add the boundaries
		pinfo.begin(be.size(), "Add boundary info to queries");
		{
			sserialize::SimpleBitVector regionSet; //in ghId
			std::vector<uint32_t> regions; //in ghId
			auto rg = std::default_random_engine();
			auto r_numRegions = std::uniform_int_distribution<int>(1, 6);
			for(std::size_t i(0), s(be.size()); i < s; ++i) {
				BenchEntry & e = be[i];
				regionSet.reset();
				regions.clear();
				sserialize::CellQueryResult cqr = cmp.cqrComplete(strs2OQ(be[i].strs));
				for(std::size_t j(0), js(cqr.cellCount()); j < js; ++j) {
					auto cell = cmp.store().geoHierarchy().cell(cqr.cellId(j));
					for(uint32_t p(0), ps(cell.parentsSize()); p < ps; ++p) {
						regionSet.set( cell.parent(p) );
					}
				}
				regions.resize(regionSet.size());
				regionSet.getSet(regions.begin());
				if (!regionSet.size()) {
					continue;
				}
				auto r_bin = std::geometric_distribution<int>(0.2);
				auto r_inbin = std::uniform_int_distribution<int>(0, regions.size()/10);
				for(std::size_t numBounds(r_numRegions(rg)); e.bounds.size() < numBounds;) {
					int bin = r_bin(rg);
					int inbin = r_inbin(rg);
					std::size_t off = bin*10+inbin;
					if (off < regions.size()) {
						e.push_back( cmp.store().geoHierarchy().regionBoundary(regions.at(off)) );
					}
				}
				pinfo(i);
			}
		}
		pinfo.end();
		
		std::vector<std::pair<uint32_t, uint32_t>> resultSizes(be.size());
		
		pinfo.begin(be.size(), "Benchmarking tree");
		for(std::size_t i(0), s(be.size()); i < s; ++i) {
			auto smp = strs2SMP(be[i].strs);
			auto gmp = bounds2GMP(be[i].bounds);
			std::vector<uint32_t> result;
			tree.find(gmp, smp, std::back_inserter(result));
			std::sort(result.begin(), result.end());
			resultSizes[i].first = result.size();
			result.clear();
			pinfo(i);
		}
		pinfo.end();
		
		pinfo.begin(be.size(), "Benchmarking OSCAR");
		for(std::size_t i(0), s(be.size()); i < s; ++i) {
			auto smp = strs2OQ(be[i].strs);
			auto gmp = bounds2OQ(be[i].bounds);
			std::string oq = smp + " (" + gmp + ")";
			sserialize::ItemIndex result = cmp.cqrComplete(oq, false, 1).flaten(1);
			resultSizes[i].second = result.size();
			pinfo(i);
		}
		pinfo.end();
	}
	Tree tree;
};

void help() {
	std::cout << "prg -i <input dir> -o <oscar dir> -t <minwise-lcg32|minwise-lcg64|minwise-sha|minwise-lcg32-dedup|minwise-lcg64-dedup|minwise-sha-dedup|stringset|qgram|qgram-dedup> -m <query> --test --bench" << std::endl;
}

int main(int argc, char ** argv) {
	Config cfg;
	Data data;
	for(int i(1); i < argc; ++i) {
		std::string token(argv[i]);
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
		else if ("--bench" == token && i+1 < argc) {
			cfg.bench = ::atoi(argv[i+1]);
			++i;
		}
	}
	
	if (cfg.indir.empty()) {
		help();
		std::cout << "No input files given" << std::endl;
		return -1;
	}
	
	if (cfg.test && cfg.oscarDir.empty()) {
		help();
		std::cout << "Testing needs oscar files" << std::endl;
		return -1;
	}
	
	data.treeData = sserialize::UByteArrayAdapter::openRo(cfg.indir + "/tree", false);
	data.traitsData = sserialize::UByteArrayAdapter::openRo(cfg.indir + "/traits", false);
	
	#ifdef SSERIALIZE_UBA_OPTIONAL_REFCOUNTING
	{
		std::cout << "Disabling data reference counting" << std::endl;
		data.treeData.disableRefCounting();
		data.traitsData.disableRefCounting();
	}
	#endif
	
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
		if (cfg.bench) {
			tcmp.bench(data.cmp, cfg.bench);
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
		if (cfg.bench) {
			tcmp.bench(data.cmp, cfg.bench);
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
		if (cfg.bench) {
			tcmp.bench(data.cmp, cfg.bench);
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
		if (cfg.bench) {
			tcmp.bench(data.cmp, cfg.bench);
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
		if (cfg.bench) {
			tcmp.bench(data.cmp, cfg.bench);
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
		if (cfg.bench) {
			tcmp.bench(data.cmp, cfg.bench);
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
		if (cfg.bench) {
			tcmp.bench(data.cmp, cfg.bench);
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
		if (cfg.bench) {
			tcmp.bench(data.cmp, cfg.bench);
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
		if (cfg.bench) {
			tcmp.bench(data.cmp, cfg.bench);
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
