#include <srtree/QGram.h>
#include <map>
#include <sserialize/utility/exceptions.h>

namespace srtree {


QGram::const_iterator::const_iterator(QGram const * d, std::size_t pos) :
m_d(d), m_pos(pos)
{}

QGram::const_iterator::~const_iterator() {}

QGram::const_iterator &
QGram::const_iterator::operator++() {
	++m_pos;
	return *this;
}

std::string
QGram::const_iterator::operator*() const {
	return m_d->at(m_pos);
}

bool
QGram::const_iterator::operator==(const_iterator const & other) const {
	return m_pos == other.m_pos;
}

bool
QGram::const_iterator::operator!=(const_iterator const & other) const {
	return m_pos != other.m_pos;
}

QGram::QGram(std::string const & base, std::size_t q) :
m_base(base),
m_q(q)
{
	if (q < 1) {
		throw sserialize::InvalidAlgorithmStateException("q < 1");
	}
}

QGram::~QGram()
{}

std::size_t const &
QGram::q() const {
	return m_q;
}

std::string const &
QGram::base() const {
	return m_base;
}

std::size_t
QGram::size() const {
	return m_base.size() + q()-1;
}

std::string
QGram::at(std::size_t i) const {
	if (i+1 < q()) { //i < q-1
		return m_base.substr(0, i+1);
	}
	else {
		return m_base.substr(i-(q()-1), q());
	}
}

QGram::const_iterator
QGram::begin() const {
	return const_iterator(this, 0);
}

QGram::const_iterator
QGram::cbegin() const {
	return const_iterator(this, 0);
}

QGram::const_iterator
QGram::end() const {
	return const_iterator(this, size());
}

QGram::const_iterator
QGram::cend() const {
	return const_iterator(this, size());
}

std::size_t
QGram::intersectionSize(QGram const & first, QGram const & second) {
	std::map<std::string, uint32_t> fs, ss;
	for(std::string x : first) {
		fs[x] += 1;
	}
	for(std::string x : second) {
		ss[x] += 1;
	}
	std::size_t result = 0;
	for(auto const & x : fs) {
		auto y = ss.find(x.first);
		if (y != ss.end()) {
			result += std::min(x.second, y->second);
		}
	}
	return result;
}

}
