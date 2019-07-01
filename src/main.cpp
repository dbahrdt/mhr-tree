#include <liboscar/StaticOsmCompleter.h>

#include "OStringSetRTree.h"
#include "OMHRTree.h"
#include "OPQGramsRTree.h"

#include <crypto++/sha.h>

enum TreeType {
	TT_INVALID=0,
	TT_MINWISE_LCG=1,
	TT_MINWISE_SHA=2,
	TT_STRINGSET=3,
	TT_QGRAM=4
};

struct Config {
	std::string infile;
	TreeType tt{TT_INVALID};
	bool check{false};
	uint32_t numThreads{0};
	uint32_t q{3};
	uint32_t hashSize{2};
};

struct BaseState {
	std::shared_ptr<liboscar::Static::OsmCompleter> cmp;
	BaseState() : cmp(std::make_shared<liboscar::Static::OsmCompleter>()) {}
};

void help() {
	std::cout << "prg -i <oscar search files>  -t <minwise-lcg|minwise-sha|stringset|qgram> --check --threads <num threads> --hashSize <num> -q <size of q-grams>" << std::endl;
}

int main(int argc, char ** argv) {
	Config cfg;
	BaseState baseState;
	sserialize::ProgressInfo pinfo;
	
	for(int i(1); i < argc; ++i) {
		std::string token(argv[i]);
		if ("-i" == token && i+1 < argc) {
			cfg.infile = std::string(argv[i+1]);
		}
		else if ("-t" == token && i+1 < argc) {
			token = std::string(argv[i+1]);
			if ("minwise-lcg" == token) {
				cfg.tt = TT_MINWISE_LCG;
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
		else if ("-h" == token) {
			help();
			return 0;
		}
		else if ("--check" == token) {
			cfg.check = true;
		}
		else if ("--threads" == token && i+1 < argc) {
			cfg.numThreads = ::atoi(argv[i+1]);
			++i;
		}
		else if ("-q" == token && i+1 < argc) {
			cfg.q = ::atoi(argv[i+1]);
			++i;
		}
		else if ("--hashSize" == token && i+1 < argc) {
			cfg.hashSize = ::atoi(argv[i+1]);
			++i;
		}
	}
	
	baseState.cmp->setAllFilesFromPrefix(cfg.infile);
	
	try {
		baseState.cmp->energize();
	}
	catch (std::exception const & e) {
		help();
		std::cerr << "ERROR: " << e.what() << std::endl;
		return -1;
	}

	if (cfg.tt == TT_MINWISE_LCG) {
		OMHRTree<srtree::detail::MinWisePermutation::LinearCongruentialHash> state(baseState.cmp, cfg.q, cfg.hashSize);
		state.create(cfg.numThreads);
		state.test();
	}
	else if (cfg.tt == TT_MINWISE_SHA) {
		OMHRTree<srtree::detail::MinWisePermutation::CryptoPPHash<CryptoPP::SHA3_64>> state(baseState.cmp, cfg.q, cfg.hashSize);
		state.create(cfg.numThreads);
		state.test();
	}
	else if (cfg.tt == TT_STRINGSET) {
		OStringSetRTree state(baseState.cmp);
		state.setCheck(cfg.check);
		state.create();
		state.test();
	}
	else if (cfg.tt == TT_QGRAM) {
		OPQGramsRTree state(baseState.cmp, cfg.q);
		state.create();
		state.test();
	}
	else {
		std::cerr << "Invalid tree type: " << cfg.tt << std::endl;
		return -1;
	}
	return 0;
}
