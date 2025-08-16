cmake_minimum_required(VERSION 3.0...3.10)
project(rehuff)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

include_directories(
	include
	external/ogg/include
)

add_executable(rehuff
	rehuff.c
	sogg.c
	headers.c
	count.c
	recode.c
	recode-headers.c
	external/ogg/src/bitwise.c
	external/ogg/src/framing.c
)

set_target_properties(rehuff PROPERTIES C_STANDARD 11)

if(WIN32)
	target_link_options(rehuff PUBLIC -static -static-libgcc)
endif()
if((${CMAKE_SYSTEM_NAME} MATCHES "Linux") OR (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD"))
	target_link_libraries(rehuff m)
endif()
