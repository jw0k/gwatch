ADD_DEFINITIONS(-DNO_VIZ -DSTDC -DNO_GZIP)
FILE(GLOB SRC_ZLIB "*.c" "*.h")
INCLUDE_DIRECTORIES(".")
ADD_LIBRARY(zlib OBJECT ${SRC_ZLIB})
if(MSVC)
	if(STATIC_CRT)
		set_target_properties(zlib
		PROPERTIES
			MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
		)
	endif()
endif()