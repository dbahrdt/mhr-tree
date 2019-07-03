#include "TestBase.h"
#include <srtree/MinWiseSignature.h>
#include <srtree/QGram.h>

#include <random>

namespace srtree::tests {

class MinWiseSignatureTest: public TestBase {
CPPUNIT_TEST_SUITE( MinWiseSignatureTest );
CPPUNIT_TEST( resemblence );
CPPUNIT_TEST_SUITE_END();
public:
	static constexpr std::size_t string_size = 10;
	static constexpr std::size_t string_count = 100;
public:
	MinWiseSignatureTest() {}
public:
	void setUp() override;
public:
	void resemblence();
private:
	std::vector<std::string> m_strs;
	MinWiseSignatureGenerator<56, 64> m_g;
};

void
MinWiseSignatureTest::setUp() {
	std::string base = "123456";
	
	auto dpos = std::uniform_int_distribution<std::size_t>(0, base.size());
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
MinWiseSignatureTest::resemblence() {
	for(std::string const & str : m_strs) {
		for(std::size_t q(1); q < 5; ++q) {
			for(std::size_t i(0), s(str.size()); i < s; ++i) {
				std::string str2 = str;
				for(std::size_t p(i), e(1); p < str.size(); p += q, ++e) {
					str2.at(p) += 1;
					QGram qg1(str, q);
					QGram qg2(str2, q);
					auto mw1 = m_g(qg1.begin(), qg1.end());
					auto mw2 = m_g(qg2.begin(), qg2.end());
					CPPUNIT_ASSERT_GREATEREQUAL(int(str.size()+(q-1))-int(e*q), int(QGram::intersectionSize(qg1, qg2)));
					CPPUNIT_ASSERT_GREATEREQUAL(QGram::intersectionSize(qg1, qg2), mw1/mw2);
					
				}
			}
		} 
	}
}

} // end namespace srtree::tests

int main(int argc, char ** argv) {
	srtree::tests::TestBase::init(argc, argv);
	CppUnit::TextUi::TestRunner runner;
	runner.addTest(  srtree::tests::MinWiseSignatureTest::suite() );
	bool ok = runner.run();
	return ok ? 0 : 1;
}
