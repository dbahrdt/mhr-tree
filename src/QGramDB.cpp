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
	SSERIALIZE_CHEAP_ASSERT_NOT_EQUAL(pos, npos);
	SSERIALIZE_CHEAP_ASSERT_EQUAL(strId, m_strId);
	SSERIALIZE_CHEAP_ASSERT_EQUAL(pos, m_pos);
}

PQGram::~PQGram()
{}

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

PQGramSet::~PQGramSet()
{}

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
	c.put(v.m_minStrLen);
	c.put(v.m_maxStrLen);
	for(PQGram const & x : v.m_d) {
		c.put( x.strId() );
	}
	c.flush();
	return dest;
}

}//end namespace detail::PQGramDB
	
	
PQGramDB::PQGramDB(uint32_t q) :
m_q(q)
{}

PQGramDB::~PQGramDB()
{}

uint32_t
PQGramDB::strId(std::string const & str) const {
	if (m_d.count(str)) {
		return m_d.at(str);
	}
	return nstr;
}

PQGramDB::PQGramSet
PQGramDB::find(std::string const & str) const {
	QGram qg(str, m_q);
	std::vector<PQGram> d;
	d.reserve(qg.size());
	for(std::size_t i(0), s(qg.size()); i < s; ++i) {
		d.emplace_back(strId(qg.at(i)), i);
	}
	std::sort(d.begin(), d.end());
	return PQGramSet(d, str.size());
}

void
PQGramDB::insert(std::string const & str) {
	QGram qg(str, m_q);
	for(std::size_t i(0), s(qg.size()); i < s; ++i) {
		auto & x = m_d[qg.at(i)];
		if (!x) {
			x = m_d.size()-1;
		}
	}
}

uint32_t
PQGramDB::baseSize(PQGramSet const & v) const {
	return v.size()+1-m_q;
}

bool
PQGramDB::nomatch(PQGramSet const & base, PQGramSet const & ref, uint32_t editDistance) const {
	if (base.empty()
		|| baseSize(ref) > base.maxStrLen()+editDistance
		|| baseSize(ref)+editDistance < base.minStrLen())
	{
		return true;
	}
	//first check the string-id based stuff
	uint32_t count = 0;
	auto it(base.data().begin());
	auto jt(ref.data().begin());
	for(; it != base.data().end() && jt != ref.data().end();) {
		if (it->strId() < jt->strId()) {
			++it;
		}
		else if (jt->strId() < it->strId()) {
			++jt;
		}
		else {
			++count;
			++it;
			++jt;
		}
	}
	//count < ref.baseSize() - 1 - (editDistance - 1)*m_q;
	//count < ref.baseSize() - 1 - editDistance*m_q + m_q;
	return count + 1 + editDistance*m_q < baseSize(ref) + m_q;
}

sserialize::UByteArrayAdapter & operator<<(sserialize::UByteArrayAdapter & dest, PQGramDB const & v) {
	std::vector<std::pair<std::string, uint32_t>> tmp(v.m_d.begin(), v.m_d.end());
	using std::sort;
	sort(tmp.begin(), tmp.end());
	return dest << v.m_q << tmp;
}
	
}//end namespace srtree
