#include "TestBase.h"
#include <srtree/MinWiseSignature.h>
#include <srtree/QGram.h>

#include <random>

namespace srtree::tests {

class QGramTest: public TestBase {
CPPUNIT_TEST_SUITE( QGramTest );
CPPUNIT_TEST( bound );
CPPUNIT_TEST_SUITE_END();
public:
	static constexpr std::size_t string_size = 10;
	static constexpr std::size_t string_count = 10000;
public:
	QGramTest() {}
public:
	void setUp() override;
public:
	void bound();
private:
	std::vector<std::string> m_strs;
};

void
QGramTest::setUp() {
	std::string base = "123456789";
	
	auto dpos = std::uniform_int_distribution<std::size_t>(0, base.size()-1);
	auto dsize = std::uniform_int_distribution<std::size_t>(5, string_size);
	auto g = std::default_random_engine();
	for(std::size_t i(0); i < string_count; ++i) {
		std::string str = "";
		for(std::size_t  j(0), s(dsize(g)); j < s; ++j) {
			str += base.at(dpos(g));
		}
		m_strs.push_back(str);
	}
}

void
QGramTest::bound() {
	for(std::string const & str : m_strs) {
		for(std::size_t q(1); q < 5; ++q) {
			for(std::size_t i(0), s(str.size()); i < s; ++i) {
				std::string str2 = str;
				for(std::size_t p(i), e(1); p < str.size(); p += q, ++e) {
					str2.at(p) += 1;
					QGram qg1(str, q);
					QGram qg2(str2, q);
					CPPUNIT_ASSERT_GREATEREQUAL(int(str.size()+(q-1))-int(e*q), int(QGram::intersectionSize(qg1, qg2)));
				}
			}
		} 
	}
}

} // end namespace srtree::tests

int main(int argc, char ** argv) {
	srtree::tests::TestBase::init(argc, argv);
	CppUnit::TextUi::TestRunner runner;
	runner.addTest(  srtree::tests::QGramTest::suite() );
	runner.eventManager().popProtector();
	bool ok = runner.run();
	return ok ? 0 : 1;
}
