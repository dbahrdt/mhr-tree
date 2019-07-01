#include <liboscar/StaticOsmCompleter.h>
#include <sserialize/strings/unicode_case_functions.h>
#include <sserialize/utility/printers.h>

#include <map>

#include <srtree/QGram.h>
#include <srtree/SRTree.h>

struct State {

	std::string
	normalize(std::string const & str) const {
		return sserialize::unicode_to_lower(str);
	}

	std::string
	keyValue(liboscar::Static::OsmKeyValueObjectStore::Item const & item, uint32_t pos) const {
		return normalize("@" + item.key(pos) + ":" + item.value(pos));
	}


	std::set<std::string>
	strings(liboscar::Static::OsmKeyValueObjectStore::Item const & item, bool inherited) const {
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
				auto cell = cmp.store().geoHierarchy().cell(cellId);
				for(uint32_t i(0), s(cell.parentsSize()); i < s; ++i) {
					regionIds.insert(
						cmp.store().geoHierarchy().ghIdToStoreId(
							cell.parent(i)
						)
					);
				}
			}
			for(uint32_t regionId : regionIds) {
				f( cmp.store().at(regionId) );
			}
		}
		return result;
	}


	std::set< std::pair<std::string, uint32_t> >
	qgrams(std::set<std::string> const & strs) const {
		std::set< std::pair<std::string, uint32_t> > result;
		for(std::string const & x : strs) {
			srtree::QGram qg(x, q);
			for(uint32_t j(0), js(qg.size()); j < js; ++j) {
				result.emplace(qg.at(j), j);
			}
		};
		return result;
	}
	uint32_t q;
	liboscar::Static::OsmCompleter cmp;
};

struct WorkerBase {
public:
	virtual ~WorkerBase() {}
	virtual void set(std::set<std::string> const & strs) = 0;
	virtual bool op(std::string const & ref) const = 0;
};

template<std::size_t T_SIGNATURE_SIZE>
struct Worker: public WorkerBase {
public:
	using Hash = srtree::detail::MinWisePermutation::LinearCongruentialHash;
// 	using Hash = srtree::detail::MinWisePermutation::CryptoPPHash<CryptoPP::SHA3_64>;
	using SignatureTraits = srtree::detail::MinWiseSignatureTraits<T_SIGNATURE_SIZE, Hash>;
public:
	~Worker() override {}
	void set(std::set<std::string> const & strs) override {
		m_sig = m_straits.signature(strs.begin(), strs.end());
	}
	bool op(std::string const & ref) const override {
		auto mhm = m_straits.mayHaveMatch(ref, 0);
		return mhm(m_sig);
	}
private:
	SignatureTraits m_straits;
	typename SignatureTraits::Signature m_sig;
};

void help() {
	std::cout << "prg <path to oscar files>" << std::endl;
}

#define ADD_WORKER(__SIGS) do { w[__SIGS].reset( new Worker<__SIGS>() ); } while(0);

int main(int argc, char ** argv) {
	if (argc < 2) {
		help();
		return -1;
	}
	std::string fn = std::string(argv[1]);
	
	State state;
	state.cmp.setAllFilesFromPrefix(fn);
	state.cmp.energize();

	std::map<uint32_t, std::unique_ptr<WorkerBase>> w;
	
	std::map<std::string, std::vector<uint32_t>> specialStrings;
	
	specialStrings["@addr:country:de"] = std::vector<uint32_t>{12085, 15559, 16571, 22974, 26027, 27209, 34220, 34931, 35083, 40110, 42801, 43766, 45429, 45430, 45432, 45434, 45467, 45473, 45474, 45475, 45476, 45481, 46383, 46394, 46396, 48618, 53607, 64157, 70438, 72101, 81659, 82359, 82360, 86704, 86741, 86742, 86743, 86744, 86749, 86751, 105813, 105889, 105932, 109461, 111825, 111827, 114070, 114330, 114331, 114332, 114333, 114334, 114335, 114343, 130062, 130336, 142216, 166140, 166142, 177996, 216057, 270062, 270063, 270064, 270085, 270087, 270635, 271289, 271290, 275230, 275231, 276169, 276170,
276178, 281984, 283816, 283990, 284870, 284871, 284931, 293376};
	specialStrings["@addr:city:bremen"] = std::vector<uint32_t>{12085, 22974, 26027, 27209, 33231, 34220, 34492, 34744, 34871, 34931, 35083, 38799, 39275, 39771, 39784, 39786, 39789, 39796, 39821, 39826, 39831, 39833, 40110, 42491, 42801, 43766, 45429, 45430, 45432, 45434, 45467, 45473, 45474, 45475, 45476, 45481, 46383, 46394, 46396, 47357, 47376, 47377, 47451, 47713, 47716, 47743, 47745, 47799, 47836, 47946, 48160, 48161, 48162, 48166, 48618, 53155, 53552, 53607, 53933, 53968, 53969, 53972, 53974, 53979, 53982, 64157, 70438, 72101, 76953, 76955, 81659, 82359, 82360, 86704, 86741, 86742, 86743, 86744, 86749, 86751, 91232, 96133, 98661, 98688, 105813, 105889, 105932, 109461, 111825, 111827, 114070, 114330, 114331, 114332, 114333, 114334, 114335, 114343, 114921, 130062, 130336, 142216, 166140, 166142, 177996, 215917, 215918, 216057, 216587, 216588, 216589, 216595, 216649, 216707, 216708, 216709, 216710, 242532, 242533, 242535, 242537, 242562, 242564, 242565, 242566, 252957,
 252958, 252990, 253039, 270062, 270063, 270064, 270085, 270087, 270581, 270621, 270622, 270623, 270635, 271289, 271290, 274652, 274653, 275230, 275231, 276169, 276170, 276178, 276528, 281984, 283816, 283990, 284870, 284871, 284931, 293376};
	
	ADD_WORKER(32);
	ADD_WORKER(64);
	ADD_WORKER(128);
	ADD_WORKER(256);
	ADD_WORKER(512);
	ADD_WORKER(1024);
	ADD_WORKER(2048);
	
	
	
	std::map<uint32_t, uint32_t> matchCount;
	

	for(auto const & str2ItemIds : specialStrings) {
		for(uint32_t itemId : str2ItemIds.second) {
			matchCount.clear();
			auto item = state.cmp.store().at(itemId);
			auto strs = state.strings(item, true);
			
			for(auto & x : w) {
				x.second->set(strs);
			}
			for(auto & x : w) {
				if (x.second->op(str2ItemIds.first)) {
					matchCount[x.first] += 1;
				}
			}
			{
				std::cout << '\xd';
				using namespace sserialize;
				std::cout << "Item " << itemId << ": " << matchCount << std::endl;
			}
		}
	}
	
	for(uint32_t itemId(state.cmp.store().geoHierarchy().regionSize()), s(state.cmp.store().size()); itemId < s; ++itemId) {
		matchCount.clear();
		auto item = state.cmp.store().at(itemId);
		auto strs = state.strings(item, true);
		
		for(auto & x : w) {
			x.second->set(strs);
		}
		uint32_t counter = 0;
		uint32_t total = strs.size() * w.size();
		for(std::string const & str : strs) {
			for(auto & x : w) {
				if (x.second->op(str)) {
					matchCount[x.first] += 1;
				}
				++counter;
				{
					std::cout << '\xd';
					using namespace sserialize;
					std::cout << "Item " << itemId << ": " << counter << '/' << total << ": " << matchCount << std::flush;
				}
			}
		}
		std::cout << std::endl;
	}
	
	return 0;
}
