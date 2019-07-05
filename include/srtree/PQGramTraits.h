#pragma once

#include <srtree/QGram.h>
#include <srtree/QGramDB.h>

#include <sserialize/algorithm/utilfunctional.h>
#include <sserialize/storage/SerializationInfo.h>

#include <memory>

namespace srtree::detail {
	
class PQGramTraits {
public:
	using Signature = PQGramDB::PQGramSet;
	
	class Serializer {
	public:
		inline sserialize::UByteArrayAdapter & operator()(sserialize::UByteArrayAdapter & dest, Signature const & v) const {
			return dest << v;
		}
	};

	class Combine {
	public:
		inline Signature operator()(Signature const & first, Signature const & second) const {
			return first + second;
		}
		template<typename Iterator>
		inline Signature operator()(Iterator begin, Iterator end) const {
			return sserialize::treeReduce<Iterator, Signature>(begin, end, *this);
		}
	};
	
	class MayHaveMatch {
	public:
		using MatchReference = PQGramDB::PQGramSet;
	public:
		MayHaveMatch(MayHaveMatch const & other);
		MayHaveMatch(MayHaveMatch && other);
		~MayHaveMatch();
	public:
		bool operator()(Signature const & ns) const;
		MayHaveMatch operator/(MayHaveMatch const & other) const;
		MayHaveMatch operator+(MayHaveMatch const & other) const;
	private:
		friend class PQGramTraits;
	private:
		MayHaveMatch(std::shared_ptr<::srtree::PQGramDB> const & db, MatchReference const & reference, uint32_t ed);
	private:
		struct Node {
			enum Type {LEAF, INTERSECT, UNITE};
			virtual ~Node() {}
			virtual bool nomatch(srtree::PQGramDB const & db, Signature const & v, uint32_t ed) = 0;
			virtual std::unique_ptr<Node> copy() const = 0;
		};
		struct IntersectNode: public Node {
			std::unique_ptr<Node> first;
			std::unique_ptr<Node> second;
			bool nomatch(srtree::PQGramDB const & db, Signature const & v, uint32_t ed) override;
			IntersectNode(std::unique_ptr<Node> && first, std::unique_ptr<Node> && second);
			~IntersectNode() override;
			std::unique_ptr<Node> copy() const override;
		};
		struct UniteNode: public Node {
			std::unique_ptr<Node> first;
			std::unique_ptr<Node> second;
			~UniteNode() override;
			bool nomatch(srtree::PQGramDB const & db, Signature const & v, uint32_t ed) override;
			
			UniteNode(std::unique_ptr<Node> && first, std::unique_ptr<Node> && second);
			std::unique_ptr<Node> copy() const override;
		};
		struct LeafNode: public Node {
			~LeafNode() override;
			bool nomatch(srtree::PQGramDB const & db, Signature const & v, uint32_t ed) override;
			LeafNode(MatchReference const & ref);
			std::unique_ptr<Node> copy() const override;
			MatchReference ref;
		};
	private:
		MayHaveMatch(std::shared_ptr<::srtree::PQGramDB> const & d, std::unique_ptr<Node> && t, uint32_t ed);
	private:
		bool nomatch(Signature const & v);
	private:
		std::shared_ptr<::srtree::PQGramDB> m_d;
		std::unique_ptr<Node> m_t;
		uint32_t m_ed;
	};
public:
	PQGramTraits(uint32_t q = 3);
	PQGramTraits(PQGramTraits const &) = default;
	PQGramTraits(PQGramTraits && other) = default;
	virtual ~PQGramTraits() {}
public:
	Combine combine() const;
	MayHaveMatch mayHaveMatch(std::string const & ref, uint32_t ed) const;
	Serializer serializer() const { return Serializer(); }
public:
	void add(const std::string & str);
	Signature signature(const std::string & str);
public:
	::srtree::PQGramDB & db() { return *m_d; }
	::srtree::PQGramDB const & db() const { return *m_d; }
private:
	std::shared_ptr<::srtree::PQGramDB> m_d;
};

inline sserialize::UByteArrayAdapter & operator<<(sserialize::UByteArrayAdapter & dest, PQGramTraits const & v) {
	return dest << v.db();
}

}//end namespace srtree::detail
