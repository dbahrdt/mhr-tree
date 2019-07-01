#pragma once
#include <string>

namespace srtree {

class QGram {
public:
	class const_iterator final {
	public:
		const_iterator(QGram const * d, std::size_t pos);
		~const_iterator();
	public:
		const_iterator & operator++();
		std::string operator*() const;
	public:
		bool operator==(const_iterator const & other) const;
		bool operator!=(const_iterator const & other) const;
	private:
		QGram const * m_d;
		std::size_t m_pos;
	};
public:
	QGram(std::string const & base, std::size_t q);
	~QGram();
public:
	std::size_t const & q() const;
	std::string const & base() const;
	std::size_t size() const;
public:
	std::string at(std::size_t i) const;
	const_iterator begin() const;
	const_iterator cbegin() const;
	const_iterator end() const;
	const_iterator cend() const;
public:
	static std::size_t intersectionSize(QGram const & first, QGram const & second);
private:
	std::string m_base;
	std::size_t m_q;
};

}//end namespace srtree
