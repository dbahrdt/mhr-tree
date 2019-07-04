cmake_minimum_required(VERSION 3.9)
project(srtree)

set(CMAKE_MODULE_PATH
	${CMAKE_SOURCE_DIR}/cmake
	${CMAKE_SOURCE_DIR}/vendor/liboscar/vendor/sserialize/cmake
)

include(add_buildtype_ultra)
include(add_buildtype_sanitize_debug)

add_subdirectory(vendor/liboscar liboscar)
add_subdirectory(tests tests)

find_package(Boost REQUIRED)
find_package(OpenMP)

set(LIB_SOURCES_CPP
	src/MinWiseSignature.cpp
	src/SRTree.cpp
	src/QGram.cpp
	src/GeoConstraint.cpp
	src/OMHRTree.cpp
	src/OStringSetRTree.cpp
	src/QGramDB.cpp
	src/OPQGramsRTree.cpp
	src/StringSetTraits.cpp
	src/PQGramTraits.cpp
	src/MinWiseSignatureTraits.cpp
	src/GeoRectGeometryTraits.cpp
	src/Static/SRTree.cpp
)

set(LIB_SOURCES_H
	include/srtree/MinWiseSignature.h
	include/srtree/SRTree.h
	include/srtree/QGram.h
	include/srtree/GeoConstraint.h
	include/srtree/QGramDB.h
	include/srtree/StringSetTraits.h
	include/srtree/PQGramTraits.h
	include/srtree/MinWiseSignatureTraits.h
	include/srtree/GeoRectGeometryTraits.h
	include/srtree/Static/SRTree.h
)

set(SOURCES_CPP
	src/main.cpp
)

set(SOURCES_H
	src/OMHRTree.h
	src/OStringSetRTree.h
	src/OPQGramsRTree.h
)

set(QUERY_SOURCES_CPP
	src/mainq.cpp
)

set(CREATE_SOURCES_CPP
	src/mainc.cpp
)

SET(LINK_LIBS
	sserialize
	liboscar
)

if (OpenMP_C_FOUND)
	set(LINK_LIBS
		${LINK_LIBS}
		OpenMP::OpenMP_C
	)
endif()

SET(INCLUDE_DIRS
	${CMAKE_SOURCE_DIR}/include
	${Boost_LIBRARIES}
)

add_library(lib${PROJECT_NAME} STATIC ${LIB_SOURCES_CPP} ${LIB_SOURCES_H})
target_link_libraries(lib${PROJECT_NAME} PUBLIC ${LINK_LIBS})
target_include_directories(lib${PROJECT_NAME} PUBLIC ${INCLUDE_DIRS})
target_compile_options(lib${PROJECT_NAME} PUBLIC "-Wall" "-Wextra")
target_compile_features(lib${PROJECT_NAME} PUBLIC cxx_std_17)

add_executable(${PROJECT_NAME} ${SOURCES_CPP} ${SOURCES_H})
target_link_libraries(${PROJECT_NAME} lib${PROJECT_NAME})

add_executable(${PROJECT_NAME}-create ${CREATE_SOURCES_CPP})
target_link_libraries(${PROJECT_NAME}-create lib${PROJECT_NAME})

add_executable(${PROJECT_NAME}-query ${QUERY_SOURCES_CPP})
target_link_libraries(${PROJECT_NAME}-query lib${PROJECT_NAME})
