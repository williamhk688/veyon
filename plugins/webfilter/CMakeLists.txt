include(BuildVeyonPlugin)

set(webfilter_sources
	WebFilterFeaturePlugin.cpp
	WebFilterFeaturePlugin.h
	WebFilterConfiguration.h
	WebFilterConfigurationPage.cpp
	WebFilterConfigurationPage.h
	WebFilterConfigurationPage.ui
	WebFilterLists.cpp
	WebFilterLists.h
	PersistentWebFilterState.cpp
	PersistentWebFilterState.h
	WebFilterEngine.cpp
	WebFilterEngine.h
	webfilter.qrc
	)

if(VEYON_BUILD_WINDOWS)
	list(APPEND webfilter_sources
		WindowsWebFilterEngine.cpp
		WindowsWebFilterIpc.cpp
		WindowsWebFilterIpc.h)
endif()

build_veyon_plugin(webfilter
	NAME WebFilter
	SOURCES ${webfilter_sources})

if(WITH_TESTS)
	add_executable(WebFilterListsTest
		WebFilterListsTest.cpp
		WebFilterLists.cpp)
	add_test(NAME WebFilterListsTest COMMAND WebFilterListsTest)
	target_include_directories(WebFilterListsTest PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
		${CMAKE_CURRENT_BINARY_DIR})
	target_link_libraries(WebFilterListsTest PRIVATE
		Qt${QT_MAJOR_VERSION}::Test
		veyon-core)

	add_executable(PersistentWebFilterStateTest
		PersistentWebFilterStateTest.cpp
		PersistentWebFilterState.cpp
		WebFilterLists.cpp)
	add_test(NAME PersistentWebFilterStateTest COMMAND PersistentWebFilterStateTest)
	target_include_directories(PersistentWebFilterStateTest PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
		${CMAKE_CURRENT_BINARY_DIR})
	target_link_libraries(PersistentWebFilterStateTest PRIVATE
		Qt${QT_MAJOR_VERSION}::Test
		veyon-core)
endif()
