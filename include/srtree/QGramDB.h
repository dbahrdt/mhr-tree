#pragma once

#include <unordered_map>
#include <vector>
#include <limits>

#include <srtree/QGram.h>

namespace srtree {
namespace detail::PQGramDB {

class PQGram;
class PQGramSet;
	
} //end namespace detail::PQGramDB

class PQGramDB final {
public:
	using StringIdType = uint32_t;
	using PositionType = uint8_t;
	using QType = uint8_t;
	static constexpr StringIdType nstr = 0xFFFFFF;
	static constexpr PositionType npos = 0xFF;
	static constexpr QType nq = 0xFF;
	using PQGramSet = detail::PQGramDB::PQGramSet;
public:
	PQGramDB(uint32_t q);
	~PQGramDB();
public:
	///returns nstr if no matching string was found
	uint32_t strId(std::string const & str) const;
	///q-grams without matching entry are inserted with nstr as strId
	PQGramSet find(std::string const & str) const;
public:
	void insert(std::string const & str);
public://PQGrams functions
	uint32_t baseSize(PQGramSet const & v) const;
public:  //PQGramSet functions
	///returns true if @param ref and the strings represented by base definetly
	///have an edit distance large than @param editDistance
	bool nomatch(PQGramSet const & base, PQGramSet const & test, uint32_t editDistance) const;
private:
	using PQGram = detail::PQGramDB::PQGram;
	using Key = std::string;
	using Value = uint32_t;
	using HashMap = std::unordered_map<Key, Value>;
private:
	HashMap m_d;
	uint32_t m_q;
};

namespace detail::PQGramDB {
	
class PQGram final {
public:
	PQGram();
	PQGram(uint32_t strId, uint32_t pos);
	~PQGram();
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
	using StringIdType = srtree::PQGramDB::StringIdType;
	using PositionType = srtree::PQGramDB::PositionType;
	using QType = srtree::PQGramDB::QType;
public:
	PQGramSet();
	PQGramSet(std::vector<PQGram> d, PositionType strLen);
	~PQGramSet();
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
	std::vector<PQGram> m_d; //sorted
	PositionType m_minStrLen{srtree::PQGramDB::npos};
	PositionType m_maxStrLen{srtree::PQGramDB::npos};
};

}//end namespace detail::PQGramDB
	
} //end namespace QGramDB
