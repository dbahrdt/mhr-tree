cmake_minimum_required(VERSION 3.9)
project(srtreetests)
find_package(CPPUNIT)

if (CPPUNIT_FOUND)

	set(TEST_LIB_SOURCES_CPP
		TestBase.cpp
	)

	add_library(${PROJECT_NAME} STATIC ${TEST_LIB_SOURCES_CPP})
	target_link_libraries(${PROJECT_NAME}
		libsrtree
		${CPPUNIT_LIBRARY}
	)

	MACRO(ADD_TEST_TARGET_SINGLE _name)
		add_executable("${PROJECT_NAME}_${_name}" "${_name}.cpp")
		target_link_libraries("${PROJECT_NAME}_${_name}" ${PROJECT_NAME})
		set_target_properties("${PROJECT_NAME}_${_name}" PROPERTIES OUTPUT_NAME ${_name})
	ENDMACRO(ADD_TEST_TARGET_SINGLE)

	ADD_TEST_TARGET_SINGLE(qgram)
	ADD_TEST_TARGET_SINGLE(mwsig)
	ADD_TEST_TARGET_SINGLE(mwsig_oscar)
else()
	message(WARNING "Unable to build tests due to missing cppunit")
endif()
