#pragma once

#include <sserialize/containers/ItemIndex.h>
#include <sserialize/containers/ItemIndexFactory.h>

namespace srtree::detail {
	
class StringSetTraits final {
public:
	using Signature = uint32_t;

	class Combine {
	public:
		inline Signature operator()(Signature const & first, Signature const & second) {
			return m_f->addIndex( m_f->indexById(first) + m_f->indexById(second) );
		}
		inline sserialize::ItemIndex operator()(sserialize::ItemIndex const & first, sserialize::ItemIndex const & second) {
			return first + second;
		}
		template<typename Iterator>
		Signature operator()(Iterator begin, Iterator end) {
			return m_f->addIndex(
				sserialize::treeReduceMap<Iterator, sserialize::ItemIndex>(begin, end, *this, [this](Signature sig) {
						return m_f->indexById(sig);
					}
				)
			);
		}
	private:
		Combine(std::shared_ptr<sserialize::ItemIndexFactory> const & idxFactory) :
		m_f(idxFactory)
		{}
	private:
		friend class StringSetTraits;
	private:
		std::shared_ptr<sserialize::ItemIndexFactory> m_f;
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
		MayHaveMatch(std::shared_ptr<sserialize::ItemIndexFactory> const & idxFactory, sserialize::ItemIndex const & reference);
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
		MayHaveMatch(std::shared_ptr<sserialize::ItemIndexFactory> const & idxFactory, std::unique_ptr<Node> && t);
	private:
		std::shared_ptr<sserialize::ItemIndexFactory> m_f;
		std::unique_ptr<Node> m_t;
	};
public:
	StringSetTraits();
	StringSetTraits(sserialize::ItemIndexFactory && idxFactory);
	StringSetTraits(StringSetTraits const &) = default;
	StringSetTraits(StringSetTraits && other);
	~StringSetTraits();
public:
	inline Combine combine() const { return Combine(m_f); }
	inline MayHaveMatch mayHaveMatch(sserialize::ItemIndex const & validStrings) const { return MayHaveMatch(m_f, validStrings); }
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
	sserialize::ItemIndexFactory & idxFactory() { return *m_f; }
	sserialize::ItemIndexFactory const & idxFactory() const { return *m_f; }
private:
	std::shared_ptr<sserialize::ItemIndexFactory> m_f;
};

	
}//end namespace srtree::detail
