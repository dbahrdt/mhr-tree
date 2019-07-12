#include <srtree/QGramDB.h>

#include <algorithm>

#include <sserialize/utility/assert.h>
#include <sserialize/utility/exceptions.h>
#include <sserialize/Static/Map.h>
#include <boost/iterator.hpp>
#include <boost/iterator/transform_iterator.hpp>

namespace srtree {

namespace detail::PQGramDB {
	
PQGram::PQGram() :
PQGram(srtree::PQGramDB::nstr, srtree::PQGramDB::npos)
{}

PQGram::PQGram(uint32_t strId, uint32_t pos) :
m_strId(strId),
m_pos(pos)
{
	SSERIALIZE_CHEAP_ASSERT_NOT_EQUAL(pos, srtree::PQGramDB::npos);
	SSERIALIZE_CHEAP_ASSERT_EQUAL(strId, m_strId);
	SSERIALIZE_CHEAP_ASSERT_EQUAL(pos, m_pos);
}

PQGram::~PQGram()
{}

bool
PQGram::operator==(PQGram const & other) const {
	return m_strId == other.m_strId && m_pos == other.m_pos;
}

bool
PQGram::operator!=(PQGram const & other) const {
	return !(*this == other);
}

uint32_t
PQGram::strId() const {
	return m_strId;
}

uint32_t
PQGram::pos() const {
	return m_pos;
}

bool
PQGram::operator<(PQGram const & other) const {
	return (m_strId == other.m_strId ? m_pos < other.m_pos : m_strId < other.m_strId);
}

PQGramSet::PQGramSet()
{}

PQGramSet::PQGramSet(std::vector<PQGram> d, PositionType len) :
m_d(std::move(d)),
m_minStrLen(srtree::PQGramDB::npos),
m_maxStrLen(len)
{}

PQGramSet::PQGramSet(sserialize::UByteArrayAdapter d) {
	sserialize::BoundedCompactUintArray positions(d);
	d += positions.getSizeInBytes();
	d >> m_minStrLen;
	d >> m_maxStrLen;
	d.shrinkToGetPtr();
	sserialize::RLEStream strIds(d);
	for(sserialize::BoundedCompactUintArray::SizeType i(0), s(positions.size()); i < s; ++i, ++strIds) {
		m_d.emplace_back(*strIds, positions.at(i));
	}
}

PQGramSet::~PQGramSet()
{}


bool
PQGramSet::operator==(PQGramSet const & other) const {
	return m_minStrLen == other.m_minStrLen && m_maxStrLen == other.m_maxStrLen && m_d == other.m_d;
}

bool
PQGramSet::operator!=(PQGramSet const & other) const {
	return !(*this == other);
}

uint32_t
PQGramSet::size() const {
	return m_d.size();
}

PQGramSet::PositionType
PQGramSet::minStrLen() const {
	return m_minStrLen;
}

PQGramSet::PositionType
PQGramSet::maxStrLen() const {
	return m_maxStrLen;
}

std::vector<PQGram> const &
PQGramSet::data() const {
	return m_d;
}

bool
PQGramSet::isSingle() const {
	return m_maxStrLen != srtree::PQGramDB::npos && m_minStrLen == srtree::PQGramDB::npos;
}

bool
PQGramSet::empty() const {
	return minStrLen() == srtree::PQGramDB::npos && maxStrLen() == srtree::PQGramDB::npos;
}

PQGramSet
PQGramSet::operator+(PQGramSet const & other) const {
	if (empty()) {
		return other;
	}
	else if (other.empty()) {
		return *this;
	}
	PQGramSet result;
	result.m_minStrLen = std::min(m_minStrLen, other.m_minStrLen);
	result.m_maxStrLen = std::max(m_maxStrLen, other.m_maxStrLen);
	std::set_union(
		m_d.begin(), m_d.end(),
		other.m_d.begin(), other.m_d.end(),
		std::back_inserter(result.m_d)
	);
	return result;
}

sserialize::UByteArrayAdapter & operator<<(sserialize::UByteArrayAdapter & dest, PQGramSet const & v) {
	{
		struct Mapper {
			PQGramSet::PositionType operator()(PQGram const & v) const {
				return v.pos();
			}
		};
		using PositionIterator = boost::transform_iterator<Mapper, PQGramSet::const_iterator>;
		PositionIterator pb(v.data().begin());
		PositionIterator pe(v.data().end());
		sserialize::BoundedCompactUintArray::create(pb, pe, dest);
	}
	dest << v.m_minStrLen;
	dest << v.m_maxStrLen;
	sserialize::RLEStream::Creator c(dest);
	for(PQGram const & x : v.m_d) {
		c.put( x.strId() );
	}
	c.flush();
	return dest;
}

}//end namespace detail::PQGramDB
	
	
PQGramDB::PQGramDB(uint32_t q) :
Parent(q)
{}

PQGramDB::~PQGramDB()
{}

void
PQGramDB::insert(std::string const & str) {
	QGram qg(str, q());
	for(std::size_t i(0), s(qg.size()); i < s; ++i) {
		auto & x = data()[qg.at(i)];
		if (!x) {
			x = data().size()-1;
		}
	}
}

sserialize::UByteArrayAdapter & operator<<(sserialize::UByteArrayAdapter & dest, PQGramDB const & v) {
	std::vector<std::pair<std::string, uint32_t>> tmp(v.data().begin(), v.data().end());
	using std::sort;
	sort(tmp.begin(), tmp.end());
	return dest << PQGramDB::QType(v.q()) << tmp;
}

namespace Static {

PQGramDB::PQGramDB(sserialize::UByteArrayAdapter const & d) :
Parent(Map(d+sserialize::SerializationInfo<QType>::length), d.get<QType>(0))
{}

sserialize::UByteArrayAdapter::SizeType
PQGramDB::getSizeInBytes() const {
	return sserialize::SerializationInfo<Map>::sizeInBytes(data()) + sserialize::SerializationInfo<QType>::sizeInBytes(q());
}

}//end namespace Static
	
}//end namespace srtree
