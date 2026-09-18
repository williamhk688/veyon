include(BuildVeyonPlugin)

build_veyon_plugin(screenlock
	NAME ScreenLock
	SOURCES
	ScreenLockFeaturePlugin.cpp ScreenLockFeaturePlugin.h
	PersistentScreenLockState.cpp PersistentScreenLockState.h
	screenlock.qrc)

if(WITH_TESTS)
	add_executable(PersistentScreenLockStateTest
		PersistentScreenLockStateTest.cpp
		PersistentScreenLockState.cpp)
	add_test(NAME PersistentScreenLockStateTest COMMAND PersistentScreenLockStateTest)
	target_include_directories(PersistentScreenLockStateTest PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
		${CMAKE_CURRENT_BINARY_DIR})
	target_link_libraries(PersistentScreenLockStateTest PRIVATE
		Qt${QT_MAJOR_VERSION}::Test
		veyon-core)
endif()
