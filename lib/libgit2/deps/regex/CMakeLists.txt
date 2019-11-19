INCLUDE_DIRECTORIES(".")
ADD_LIBRARY(regex OBJECT "regex.c" "regex.h")
if(MSVC)
	if(STATIC_CRT)
		set_target_properties(regex
		PROPERTIES
			MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
		)
	endif()
endif()