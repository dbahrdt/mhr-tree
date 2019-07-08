#include <srtree/PQGramTraits.h>

namespace srtree::detail {

void
PQGramTraits::add(const std::string & str) {
	db().insert(str);
}


	
}//end namespace
