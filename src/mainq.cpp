#include <srtree/Static/SRTree.h>
#include <srtree/PQGramTraits.h>
#include <srtree/Static/DedupDeserializationTraitsAdapter.h>
#include <liboscar/AdvancedOpTree.h>
#include <sserialize/containers/ItemIndex.h>

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
	sserialize::UByteArrayAdapter treeData;
	sserialize::UByteArrayAdapter traitsData;
	TreeType tt;
	std::vector<std::string> queries;
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
		tree = std::move( Tree(treeData, std::move(straits), gtraits) );
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
	Tree tree;
};

void help() {
	std::cout << "prg -i <input dir> -t <minwise-lcg|minwise-sha|stringset|qgram>" << std::endl;
}

int main(int argc, char ** argv) {
	Config cfg;
	for(int i(1); i < argc; ++i) {
		std::string token(argv[i+1]);
		if (token == "-i" && i+1 < argc) {
			cfg.indir = std::string(argv[i+1]);
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
	}
	switch(cfg.tt) {
	case TT_MINWISE_LCG_32:
	{
		constexpr std::size_t SignatureSize = 56;
		using Hash = srtree::detail::MinWisePermutation::LinearCongruentialHash<32>;
		using Traits = srtree::detail::MinWiseSignatureTraits<SignatureSize, Hash>;
		Completer<Traits> tcmp(cfg.treeData, cfg.traitsData);
		tcmp.complete(cfg.queries);
	}
		break;
	case TT_MINWISE_LCG_64:
	{
		constexpr std::size_t SignatureSize = 56;
		using Hash = srtree::detail::MinWisePermutation::LinearCongruentialHash<64>;
		using Traits = srtree::detail::MinWiseSignatureTraits<SignatureSize, Hash>;
		Completer<Traits> tcmp(cfg.treeData, cfg.traitsData);
		tcmp.complete(cfg.queries);
	}
		break;
	case TT_MINWISE_SHA:
	{
		constexpr std::size_t SignatureSize = 56;
		using Hash = srtree::detail::MinWisePermutation::CryptoPPHash<CryptoPP::SHA3_64>;
		using Traits = srtree::detail::MinWiseSignatureTraits<SignatureSize, Hash>;
		Completer<Traits> tcmp(cfg.treeData, cfg.traitsData);
		tcmp.complete(cfg.queries);
	}
		break;
	case TT_MINWISE_LCG_32_DEDUP:
	{
		constexpr std::size_t SignatureSize = 56;
		using Hash = srtree::detail::MinWisePermutation::LinearCongruentialHash<32>;
		using BaseTraits = srtree::detail::MinWiseSignatureTraits<SignatureSize, Hash>;
		using Traits = srtree::detail::DedupDeserializationTraitsAdapter<BaseTraits>;
		Completer<Traits> tcmp(cfg.treeData, cfg.traitsData);
		tcmp.complete(cfg.queries);
	}
		break;
	case TT_MINWISE_LCG_64_DEDUP:
	{
		constexpr std::size_t SignatureSize = 56;
		using Hash = srtree::detail::MinWisePermutation::LinearCongruentialHash<64>;
		using BaseTraits = srtree::detail::MinWiseSignatureTraits<SignatureSize, Hash>;
		using Traits = srtree::detail::DedupDeserializationTraitsAdapter<BaseTraits>;
		Completer<Traits> tcmp(cfg.treeData, cfg.traitsData);
		tcmp.complete(cfg.queries);
	}
		break;
	case TT_MINWISE_SHA_DEDUP:
	{
		constexpr std::size_t SignatureSize = 56;
		using Hash = srtree::detail::MinWisePermutation::CryptoPPHash<CryptoPP::SHA3_64>;
		using BaseTraits = srtree::detail::MinWiseSignatureTraits<SignatureSize, Hash>;
		using Traits = srtree::detail::DedupDeserializationTraitsAdapter<BaseTraits>;
		Completer<Traits> tcmp(cfg.treeData, cfg.traitsData);
		tcmp.complete(cfg.queries);
	}
		break;
// 	case TT_STRINGSET:
// 	{
// 		OStringSetRTree state(baseState.cmp);
// 		state.setCheck(cfg.check);
// 		state.create();
// 		state.serialize(baseState.treeData, baseState.traitsData);
// 	}
// 		break;
	case TT_QGRAM:
	{
		using Traits = srtree::Static::detail::PQGramTraits;
		Completer<Traits> tcmp(cfg.treeData, cfg.traitsData);
		tcmp.complete(cfg.queries);
	}
		break;
	case TT_QGRAM_DEDUP:
	{
		using BaseTraits = srtree::Static::detail::PQGramTraits;
		using Traits = srtree::detail::DedupDeserializationTraitsAdapter<BaseTraits>;
		Completer<Traits> tcmp(cfg.treeData, cfg.traitsData);
		tcmp.complete(cfg.queries);
	}
		break;
	default:
		std::cerr << "Invalid tree type: " << cfg.tt << std::endl;
		return -1;
	}
	return 0;
}
