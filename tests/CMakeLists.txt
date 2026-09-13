# Each suite is guarded on the modules it links. The module switches are an advertised feature, and
# a test tree that ignored them turned any subset build into a link error -- which is how
# -DCATALYST_BUILD_AUDIO=OFF used to fail: not at configure time with a clear message, but much
# later, building a test for a module that was not there.

if(CATALYST_BUILD_AUDIO)
	add_subdirectory(audio)
endif()

if(CATALYST_BUILD_EVENTS)
	add_subdirectory(events)
endif()

if(CATALYST_BUILD_INPUT)
	add_subdirectory(input)
endif()

if(CATALYST_BUILD_LOGGING)
	add_subdirectory(logging)
endif()

if(CATALYST_BUILD_MATH)
	add_subdirectory(math)
endif()

if(CATALYST_BUILD_RENDERING)
	add_subdirectory(rendering)
endif()

if(CATALYST_BUILD_RESOURCE)
	add_subdirectory(resource)
endif()

if(CATALYST_BUILD_TEXT)
	add_subdirectory(text)
endif()

if(CATALYST_BUILD_UI)
	add_subdirectory(ui)
endif()
