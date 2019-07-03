#include <liboscar/StaticOsmCompleter.h>

#include "OStringSetRTree.h"
#include "OMHRTree.h"
#include "OPQGramsRTree.h"

#include <crypto++/sha.h>

enum TreeType {
	TT_INVALID,
	TT_MINWISE_LCG_32,
	TT_MINWISE_LCG_64,
	TT_MINWISE_SHA,
	TT_STRINGSET,
	TT_QGRAM
};

struct Config {
	std::string infile;
	std::string outdir;
	TreeType tt{TT_INVALID};
	bool check{false};
	uint32_t numThreads{0};
	uint32_t q{3};
	uint32_t hashSize{2};
};

struct BaseState {
	std::shared_ptr<liboscar::Static::OsmCompleter> cmp;
	BaseState() : cmp(std::make_shared<liboscar::Static::OsmCompleter>()) {}
	
	sserialize::UByteArrayAdapter treeData;
	sserialize::UByteArrayAdapter traitsData;
};

void help() {
	std::cout << "prg -i <oscar search files> -o <path to srtree files> -t <minwise-lcg32|minwise-lcg64|minwise-sha|stringset|qgram> --check --threads <num threads> --hashSize <num> -q <size of q-grams>" << std::endl;
}

int main(int argc, char ** argv) {
	Config cfg;
	BaseState baseState;
	sserialize::ProgressInfo pinfo;
	
	for(int i(1); i < argc; ++i) {
		std::string token(argv[i]);
		if ("-i" == token && i+1 < argc) {
			cfg.infile = std::string(argv[i+1]);
			++i;
		}
		else if ("-o" == token && i+1 < argc) {
			cfg.outdir = std::string(argv[i+1]);
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
			++i;
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
	
	if (cfg.outdir.empty()) {
		help();
		std::cerr << "No outdir set" << std::endl;
		return -1;
	}
	
	if (sserialize::MmappedFile::fileExists(cfg.outdir)) {
		if (!sserialize::MmappedFile::isDirectory(cfg.outdir)) {
			std::cerr << "Outdir already exists and is not a directory" << std::endl;
			return -1;
		}
	}
	else {
		if (!sserialize::MmappedFile::createDirectory(cfg.outdir)) {
			std::cerr << "Could not create directory at " << cfg.outdir << std::endl;
			return -1;
		}
	}
	
	baseState.treeData = sserialize::UByteArrayAdapter::createFile(0, cfg.outdir + "/tree");
	baseState.traitsData = sserialize::UByteArrayAdapter::createFile(0, cfg.outdir + "/traits");
	
	baseState.cmp->setAllFilesFromPrefix(cfg.infile);
	
	try {
		baseState.cmp->energize();
	}
	catch (std::exception const & e) {
		help();
		std::cerr << "ERROR: " << e.what() << std::endl;
		return -1;
	}

	if (cfg.tt == TT_MINWISE_LCG_32) {
		OMHRTree<srtree::detail::MinWisePermutation::LinearCongruentialHash<32>> state(baseState.cmp, cfg.q, cfg.hashSize);
		state.create(cfg.numThreads);
		state.serialize(baseState.treeData, baseState.traitsData);
	}
	else if (cfg.tt == TT_MINWISE_LCG_64) {
		OMHRTree<srtree::detail::MinWisePermutation::LinearCongruentialHash<64>> state(baseState.cmp, cfg.q, cfg.hashSize);
		state.create(cfg.numThreads);
		state.serialize(baseState.treeData, baseState.traitsData);
	}
	else if (cfg.tt == TT_MINWISE_SHA) {
		OMHRTree<srtree::detail::MinWisePermutation::CryptoPPHash<CryptoPP::SHA3_64>> state(baseState.cmp, cfg.q, cfg.hashSize);
		state.create(cfg.numThreads);
		state.serialize(baseState.treeData, baseState.traitsData);
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
