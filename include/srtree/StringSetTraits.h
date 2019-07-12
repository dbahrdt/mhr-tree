#pragma once

#include <sserialize/containers/ItemIndex.h>
#include <sserialize/containers/ItemIndexFactory.h>
#include <sserialize/containers/HashBasedFlatTrie.h>

#include <srtree/Static/StringSetTraits.h>

namespace srtree::detail {
	
class StringSetTraits final {
private:
	struct StringId {
		static constexpr uint32_t Invalid = std::numeric_limits<uint32_t>::max();
		static constexpr uint32_t Internal = Invalid-1;
		static constexpr uint32_t GenericLeaf = Internal-1;
		
		bool valid() const { return value != Invalid; }
		bool internal() const { return value == Internal; }
		bool leaf() const { return value < Internal; }
		
		uint32_t value{Invalid};
	};
	using String2IdMap = sserialize::HashBasedFlatTrie<StringId>;
	struct Data {
		sserialize::ItemIndexFactory idxFactory;
		String2IdMap str2Id;
		Data() {}
		Data(sserialize::ItemIndexFactory && idxFactory) :
		idxFactory(std::move(idxFactory))
		{}
	};
	using DataPtr = std::shared_ptr<Data>;
public:
	using StaticTraits = srtree::Static::detail::StringSetTraits;
public:
	using Signature = uint32_t;
	
	class Serializer {
	public:
		using Type = Signature;
	public:
		inline sserialize::UByteArrayAdapter & operator()(sserialize::UByteArrayAdapter & dest, Signature const & v) const {
			return dest << v;
		}
	};
	
	class Deserializer {
	public:
		using Type = uint32_t;
	public:
		Signature operator()(Type v) const {
			return v;
		}
	};

	class Combine {
	public:
		inline Signature operator()(Signature const & first, Signature const & second) {
			return m_d->idxFactory.addIndex( m_d->idxFactory.indexById(first) + m_d->idxFactory.indexById(second) );
		}
		inline sserialize::ItemIndex operator()(sserialize::ItemIndex const & first, sserialize::ItemIndex const & second) {
			return first + second;
		}
		template<typename Iterator>
		Signature operator()(Iterator begin, Iterator end) {
			return m_d->idxFactory.addIndex(
				sserialize::treeReduceMap<Iterator, sserialize::ItemIndex>(begin, end, *this, [this](Signature sig) {
						return m_d->idxFactory.indexById(sig);
					}
				)
			);
		}
	private:
		Combine(DataPtr const & d) :
		m_d(d)
		{}
	private:
		friend class StringSetTraits;
	private:
		DataPtr m_d;
	};
	
	class MayHaveMatch {
	public:
		MayHaveMatch(MayHaveMatch const & other);
		MayHaveMatch(MayHaveMatch && other);
		~MayHaveMatch();
	public:
		bool operator()(Signature const & ns);
		bool operator()(sserialize::ItemIndex const & ns);
		MayHaveMatch operator/(MayHaveMatch const & other) const;
		MayHaveMatch operator+(MayHaveMatch const & other) const;
	private:
		friend class StringSetTraits;
	private:
		MayHaveMatch(DataPtr const & d, sserialize::ItemIndex const & reference);
	private:
		struct Node {
			enum Type {LEAF, INTERSECT, UNITE};
			virtual ~Node() {}
			virtual bool matches(sserialize::ItemIndex const & v) = 0;
			virtual std::unique_ptr<Node> copy() const = 0;
		};
		struct IntersectNode: public Node {
			std::unique_ptr<Node> first;
			std::unique_ptr<Node> second;
			bool matches(sserialize::ItemIndex const & v) override;
			IntersectNode(std::unique_ptr<Node> && first, std::unique_ptr<Node> && second);
			~IntersectNode() override;
			std::unique_ptr<Node> copy() const override;
		};
		struct UniteNode: public Node {
			std::unique_ptr<Node> first;
			std::unique_ptr<Node> second;
			~UniteNode() override;
			bool matches(sserialize::ItemIndex const & v) override;
			
			UniteNode(std::unique_ptr<Node> && first, std::unique_ptr<Node> && second);
			std::unique_ptr<Node> copy() const override;
		};
		struct LeafNode: public Node {
			~LeafNode() override;
			bool matches(sserialize::ItemIndex const & v) override;
			LeafNode(sserialize::ItemIndex const & ref);
			std::unique_ptr<Node> copy() const override;
			sserialize::ItemIndex ref;
		};
	private:
		MayHaveMatch(DataPtr const & d, std::unique_ptr<Node> && t);
	private:
		DataPtr m_d;
		std::unique_ptr<Node> m_t;
	};
public:
	StringSetTraits();
	StringSetTraits(sserialize::ItemIndexFactory && idxFactory);
	StringSetTraits(StringSetTraits const &) = default;
	StringSetTraits(StringSetTraits && other);
	~StringSetTraits();
public:
	inline Combine combine() const { return Combine(m_d); }
	inline MayHaveMatch mayHaveMatch(sserialize::ItemIndex const & validStrings) const { return MayHaveMatch(m_d, validStrings); }
	inline Serializer serializer() const { return Serializer(); }
	inline Deserializer deserializer() const { return Deserializer(); }
public:
	void addString(std::string const & str);
	void finalizeStringTable();
	uint32_t strId(std::string const & str) const;
public:
	Signature addSignature(uint32_t stringId) {
		return addSignature( sserialize::ItemIndex(std::vector<uint32_t>(stringId, 1)) );
	}
	Signature addSignature(sserialize::ItemIndex const & strIdSet) {
		return idxFactory().addIndex(strIdSet); 
	}
	template<typename T_STRING_ID_ITERATOR>
	Signature addSignature(T_STRING_ID_ITERATOR begin, T_STRING_ID_ITERATOR end) {
		return idxFactory().addIndex(begin, end);
	}
public:
	sserialize::ItemIndexFactory & idxFactory() { return m_d->idxFactory; }
	sserialize::ItemIndexFactory const & idxFactory() const { return m_d->idxFactory; }
	sserialize::HashBasedFlatTrie<StringId> & str2Id() { return m_d->str2Id; }
	sserialize::HashBasedFlatTrie<StringId> const & str2Id() const { return m_d->str2Id; }
private:
	friend sserialize::UByteArrayAdapter & operator<<(sserialize::UByteArrayAdapter & dest, StringSetTraits & traits);
private:
	std::shared_ptr<Data> m_d;
};

sserialize::UByteArrayAdapter & operator<<(sserialize::UByteArrayAdapter & dest, StringSetTraits & traits);
	
}//end namespace srtree::detail
