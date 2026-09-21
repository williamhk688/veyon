include(BuildVeyonPlugin)

build_veyon_plugin(failsafe
	NAME FailsafePassword
	SOURCES
	FailsafePasswordFeaturePlugin.cpp
	FailsafePasswordFeaturePlugin.h
	TeacherSelfRescue.cpp
	TeacherSelfRescue.h
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

	add_executable(FailsafePasswordKeyFilterTest
		FailsafePasswordKeyFilterTest.cpp)
	add_test(NAME FailsafePasswordKeyFilterTest COMMAND FailsafePasswordKeyFilterTest)
	target_include_directories(FailsafePasswordKeyFilterTest PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
		${CMAKE_CURRENT_BINARY_DIR})
	target_link_libraries(FailsafePasswordKeyFilterTest PRIVATE
		Qt${QT_MAJOR_VERSION}::Test
		veyon-core)

	add_executable(TeacherSelfRescueTest
		TeacherSelfRescueTest.cpp
		TeacherSelfRescue.cpp)
	add_test(NAME TeacherSelfRescueTest COMMAND TeacherSelfRescueTest)
	target_include_directories(TeacherSelfRescueTest PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
		${CMAKE_CURRENT_BINARY_DIR})
	target_link_libraries(TeacherSelfRescueTest PRIVATE
		Qt${QT_MAJOR_VERSION}::Test
		Qt${QT_MAJOR_VERSION}::Widgets
		veyon-core)
endif()
