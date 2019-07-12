#pragma once

#include <unordered_map>
#include <vector>
#include <limits>

#include <srtree/QGram.h>

#include <sserialize/storage/UByteArrayAdapter.h>
#include <sserialize/containers/RLEStream.h>
#include <sserialize/containers/CompactUintArray.h>
#include <sserialize/Static/Map.h>

namespace srtree {
namespace detail::PQGramDB {

class PQGram;
class PQGramSet;
	
} //end namespace detail::PQGramDB

class PQGramDBBase {
public:
	using StringIdType = uint32_t;
	using PositionType = uint8_t;
	using QType = uint8_t;
	static constexpr StringIdType nstr = 0xFFFFFF;
	static constexpr PositionType npos = 0xFF;
	static constexpr QType nq = 0xFF;
	using PQGram = detail::PQGramDB::PQGram;
	using PQGramSet = detail::PQGramDB::PQGramSet;
public:
	virtual ~PQGramDBBase() {}
};

template<typename T_MAP>
class ROPQGramDB: public PQGramDBBase {
public:
	using Map = T_MAP;
	using key_type = typename Map::key_type;
	using value_type = typename Map::value_type;
public:
	ROPQGramDB() {}
	ROPQGramDB(ROPQGramDB const &) = default;
	ROPQGramDB(ROPQGramDB &&) = default;
	ROPQGramDB(uint32_t q) : m_q(q) {}
	ROPQGramDB(Map const & d, uint32_t q) : m_d(d), m_q(q) {}
	ROPQGramDB(Map && d, uint32_t q) : m_d(std::move(d)), m_q(q) {}
	~ROPQGramDB() override {}
	ROPQGramDB & operator=(ROPQGramDB const &) = default;
	ROPQGramDB & operator=(ROPQGramDB &&) = default;
public:
	inline QType q() const { return m_q; }
public:
	///returns nstr if no matching string was found
	uint32_t strId(std::string const & str) const;
	///q-grams without matching entry are inserted with nstr as strId
	PQGramSet find(std::string const & str) const;
public://PQGrams functions
	uint32_t baseSize(PQGramSet const & v) const;
public:  //PQGramSet functions
	///returns true if @param ref and the strings represented by base definetly
	///have an edit distance large than @param editDistance
	bool nomatch(PQGramSet const & base, PQGramSet const & test, uint32_t editDistance) const;
protected:
	inline Map const & data() const { return m_d; }
	inline Map & data() { return m_d;}
private:
	Map m_d;
	QType m_q;
};

class PQGramDB: public ROPQGramDB< std::unordered_map<std::string, uint32_t> > {
public:
	using Parent = ROPQGramDB< std::unordered_map<std::string, uint32_t> >;
public:
	PQGramDB(uint32_t q);
	PQGramDB(PQGramDB const &) = default;
	PQGramDB(PQGramDB &&) = default;
	~PQGramDB() override;
	PQGramDB & operator=(PQGramDB const &) = default;
	PQGramDB & operator=(PQGramDB &&) = default;
public:
	void insert(std::string const & str);
private:
	friend sserialize::UByteArrayAdapter & operator<<(sserialize::UByteArrayAdapter & dest, PQGramDB const & v);
};

sserialize::UByteArrayAdapter & operator<<(sserialize::UByteArrayAdapter & dest, PQGramDB const & v);

namespace Static {
	
class PQGramDB: public ROPQGramDB< sserialize::Static::Map<std::string, uint32_t> > {
public:
	using Parent = ROPQGramDB< sserialize::Static::Map<std::string, uint32_t> >;
public:
	PQGramDB() {}
	PQGramDB(sserialize::UByteArrayAdapter const & d);
	PQGramDB(PQGramDB const &) = default;
	PQGramDB(PQGramDB &&) = default;
	~PQGramDB() override {}
	PQGramDB & operator=(PQGramDB const &) = default;
	PQGramDB & operator=(PQGramDB &&) = default;
public:
	sserialize::UByteArrayAdapter::SizeType getSizeInBytes() const;
};

}//end namespace Static

namespace detail::PQGramDB {
	
class PQGram final {
public:
	PQGram();
	PQGram(uint32_t strId, uint32_t pos);
	~PQGram();
public:
	bool operator==(PQGram const &) const;
	bool operator!=(PQGram const &) const;
public:
	uint32_t strId() const;
	uint32_t pos() const;
public:
	bool operator<(PQGram const & other) const;
private:
	uint32_t m_strId:24;
	uint32_t m_pos:8;
};

///positional q-grams of a set of strings or a single string

class __attribute__((aligned(1))) PQGramSet final {
public:
	using StringIdType = srtree::PQGramDBBase::StringIdType;
	using PositionType = srtree::PQGramDBBase::PositionType;
	using QType = srtree::PQGramDBBase::QType;
	using const_iterator = std::vector<PQGram>::const_iterator;
public:
	PQGramSet();
	PQGramSet(sserialize::UByteArrayAdapter d);
	PQGramSet(std::vector<PQGram> d, PositionType strLen);
	~PQGramSet();
public:
	bool operator==(PQGramSet const & other) const;
	bool operator!=(PQGramSet const & other) const;
public:
	uint32_t size() const;
	PositionType minStrLen() const;
	PositionType maxStrLen() const;
	std::vector<PQGram> const & data() const;
public:
	bool isSingle() const;
	bool empty() const;
public:
	PQGramSet operator+(PQGramSet const & other) const;
private:
	friend sserialize::UByteArrayAdapter & operator<<(sserialize::UByteArrayAdapter & dest, PQGramSet const & v);
private:
	std::vector<PQGram> m_d; //sorted
	PositionType m_minStrLen{srtree::PQGramDB::npos};
	PositionType m_maxStrLen{srtree::PQGramDB::npos};
};

sserialize::UByteArrayAdapter & operator<<(sserialize::UByteArrayAdapter & dest, PQGramSet const & v);

}//end namespace detail::PQGramDB
	
} //end namespace QGramDB

//Implementation

namespace srtree {

#define ROPQGRAMDB_HDR template<typename T_MAP>
#define ROPQGRAMDB_CLS ROPQGramDB<T_MAP>

ROPQGRAMDB_HDR
uint32_t
ROPQGRAMDB_CLS::strId(std::string const & str) const {
	if (m_d.count(str)) {
		return m_d.at(str);
	}
	return nstr;
}
ROPQGRAMDB_HDR
PQGramDBBase::PQGramSet
ROPQGRAMDB_CLS::find(std::string const & str) const {
	QGram qg(str, m_q);
	std::vector<PQGram> d;
	d.reserve(qg.size());
	for(std::size_t i(0), s(qg.size()); i < s; ++i) {
		d.emplace_back(strId(qg.at(i)), i);
	}
	std::sort(d.begin(), d.end());
	return PQGramSet(d, str.size());
}

ROPQGRAMDB_HDR
uint32_t
ROPQGRAMDB_CLS::baseSize(PQGramSet const & v) const {
	return v.size()+1-m_q;
}

ROPQGRAMDB_HDR
bool
ROPQGRAMDB_CLS::nomatch(PQGramSet const & base, PQGramSet const & ref, uint32_t editDistance) const {
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

#undef ROPQGRAMDB_HDR
#undef ROPQGRAMDB_CLS

}//end namespace srtree
