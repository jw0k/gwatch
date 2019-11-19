FILE(GLOB SRC_HTTP "*.c" "*.h")

ADD_LIBRARY(http-parser OBJECT ${SRC_HTTP})
if(MSVC)
	if(STATIC_CRT)
		set_target_properties(http-parser
		PROPERTIES
			MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
		)
	endif()
endif()
