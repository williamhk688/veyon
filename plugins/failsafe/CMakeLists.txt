include(BuildVeyonPlugin)

build_veyon_plugin(failsafe
	NAME FailsafePassword
	SOURCES
	FailsafePasswordFeaturePlugin.cpp
	FailsafePasswordFeaturePlugin.h
	)

if(WITH_TESTS)
	add_executable(FailsafePasswordStateTest
		FailsafePasswordStateTest.cpp)
	add_test(NAME FailsafePasswordStateTest COMMAND FailsafePasswordStateTest)
	target_include_directories(FailsafePasswordStateTest PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
		${CMAKE_CURRENT_BINARY_DIR})
	target_link_libraries(FailsafePasswordStateTest PRIVATE
		Qt${QT_MAJOR_VERSION}::Test
		veyon-core)
endif()
