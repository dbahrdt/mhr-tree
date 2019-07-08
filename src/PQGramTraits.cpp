#include <srtree/PQGramTraits.h>

namespace srtree::detail {

void
PQGramTraits::add(const std::string & str) {
	db().insert(str);
}

}//end namespace

namespace srtree::Static::detail {

sserialize::UByteArrayAdapter & operator>>(sserialize::UByteArrayAdapter & src, PQGramTraits & dest) {
	sserialize::UByteArrayAdapter tmp(src);
	tmp.shrinkToGetPtr();
	dest = PQGramTraits(tmp);
	src.incGetPtr(sserialize::SerializationInfo<PQGramTraits>::sizeInBytes(dest));
	return src;
}

}
