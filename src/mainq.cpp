#include <srtree/Static/SRTree.h>

enum TreeType {
	TT_INVALID,
	TT_MINWISE_LCG_32,
	TT_MINWISE_LCG_64,
	TT_MINWISE_SHA,
	TT_STRINGSET,
	TT_QGRAM
};

struct Config {
	std::string indir;
	sserialize::UByteArrayAdapter treeData;
	sserialize::UByteArrayAdapter traitsData;
	TreeType tt;
};

template<typename T_HASH_FUNCTION>
struct SOMHRTree {
	static constexpr uint32_t SignatureSize = 56;
	using Tree = srtree::Static::SRTree<
		srtree::detail::MinWiseSignatureTraits<SignatureSize, T_HASH_FUNCTION>,
		srtree::detail::GeoRectGeometryTraits
	>;
	using SignatureTraits = typename Tree::SignatureTraits;
	using GeometryTraits = typename Tree::GeometryTraits;
	
	SOMHRTree(sserialize::UByteArrayAdapter treeData, sserialize::UByteArrayAdapter traitsData) {
		SignatureTraits straits;
		GeometryTraits gtraits;
		traitsData >> straits >> gtraits;
		tree = std::move( Tree(treeData, std::move(straits), gtraits) );
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
			if ("minwise-lcg64" == token) {
				cfg.tt = TT_MINWISE_LCG_64;
			}
			else if ("minwise-sha" == token) {
				cfg.tt = TT_MINWISE_SHA;
			}
			else if ("stringset" == token) {
				cfg.tt = TT_STRINGSET;
			}
			else if ("qgram" == token) {
				cfg.tt = TT_QGRAM;
			}
			else {
				help();
				std::cerr << "Invalid tree type: " << token << std::endl;
				return -1;
			}
		}
		
	}
	
	if (cfg.tt == TT_MINWISE_LCG_32) {
		SOMHRTree<srtree::detail::MinWisePermutation::LinearCongruentialHash<32>> state(cfg.treeData, cfg.traitsData);
		
	}
	else if (cfg.tt == TT_MINWISE_LCG_64) {
		SOMHRTree<srtree::detail::MinWisePermutation::LinearCongruentialHash<64>> state(cfg.treeData, cfg.traitsData);
		
	}
	else if (cfg.tt == TT_MINWISE_SHA) {
		SOMHRTree<srtree::detail::MinWisePermutation::CryptoPPHash<CryptoPP::SHA3_64>> state(cfg.treeData, cfg.traitsData);
	}
	else if (cfg.tt == TT_STRINGSET) {
		throw sserialize::UnsupportedFeatureException("StringSet tree");
	}
	else if (cfg.tt == TT_QGRAM) {
		throw sserialize::UnsupportedFeatureException("QGram index");
	}
	else {
		std::cerr << "Invalid tree type: " << cfg.tt << std::endl;
		return -1;
	}
	return 0;
}
