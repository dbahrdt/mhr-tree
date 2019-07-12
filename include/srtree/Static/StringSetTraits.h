#pragma once

#include <sserialize/Static/ItemIndexStore.h>
#include <sserialize/Static/UnicodeTrie/FlatTrie.h>

namespace srtree::Static::detail {
	
/**
 * struct StringSetTraits: Version(1) {
 *   sserialize::Static::ItemIndexStore idxStore;
 *   sserialize::Static::UnicodeTrie::FlatTrie<uint32_t> str2Id;
 * }
 */
	
class StringSetTraits final {
private:
	struct Data {
		sserialize::Static::ItemIndexStore idxStore;
		sserialize::Static::UnicodeTrie::FlatTrie<uint32_t> str2Id;
		Data(sserialize::UByteArrayAdapter const & d);
		sserialize::UByteArrayAdapter::SizeType getSizeInBytes() const;
	};
	using DataPtr = std::shared_ptr<Data>;
public:
	using StaticTraits = StringSetTraits;
public:
	using Signature = uint32_t;

	class Deserializer {
	public:
		using Type = Signature;
	public:
		inline Signature operator()(Type v) const {
			return v;
		}
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
	StringSetTraits(sserialize::UByteArrayAdapter const & d);
	StringSetTraits(StringSetTraits const &) = default;
	StringSetTraits(StringSetTraits && other) = default;
	~StringSetTraits();
	StringSetTraits & operator=(StringSetTraits const &) = default;
	StringSetTraits & operator=(StringSetTraits &&) = default;
	sserialize::UByteArrayAdapter::SizeType getSizeInBytes() const;
public:
	MayHaveMatch mayHaveMatch(std::string const & str, uint32_t editDistance) const;
	inline Deserializer deserializer() const { return Deserializer(); }
public:
	uint32_t strId(std::string const & str) const;
private:
	std::shared_ptr<Data> m_d;
};

}//end namespace srtree::detail
