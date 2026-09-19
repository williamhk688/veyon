include(BuildVeyonPlugin)

build_veyon_plugin(demo
	NAME Demo
	SOURCES
	DemoFeaturePlugin.cpp
	DemoConfigurationPage.cpp
	DemoConfigurationPage.ui
	DemoServer.cpp
	DemoServerConnection.cpp
	DemoServerProtocol.cpp
	DemoClient.cpp
	PersistentDemoState.cpp
	PersistentDemoState.h
	DemoFeaturePlugin.h
	DemoConfiguration.h
	DemoConfigurationPage.h
	DemoServer.h
	DemoServerConnection.h
	DemoServerProtocol.h
	DemoClient.h
	demo.qrc
	)

if(WITH_TESTS)
	add_executable(PersistentDemoStateTest
		PersistentDemoStateTest.cpp
		PersistentDemoState.cpp)
	add_test(NAME PersistentDemoStateTest COMMAND PersistentDemoStateTest)
	target_include_directories(PersistentDemoStateTest PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
		${CMAKE_CURRENT_BINARY_DIR})
	target_link_libraries(PersistentDemoStateTest PRIVATE
		Qt${QT_MAJOR_VERSION}::Test
		veyon-core)
endif()
