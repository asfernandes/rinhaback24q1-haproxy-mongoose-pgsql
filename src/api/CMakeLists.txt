set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(rinhaback24q1-haproxy-mongoose-pgsql-api CXX)

file(GLOB_RECURSE SRC
	"*.h"
	"*.cpp"
)

find_package(libpqxx REQUIRED)
find_package(mimalloc REQUIRED)
find_package(unofficial-mongoose REQUIRED)
find_package(yyjson REQUIRED)

find_path(TASK_THREAD_POOL_INCLUDE_DIRS "task_thread_pool.hpp")


foreach(item ${SRC})
	if(${item} MATCHES ".*\.test\.cpp$" OR ${item} MATCHES ".*main\.cpp$")
		list(REMOVE_ITEM SRC ${item})
	endif()
endforeach()


add_library(${PROJECT_NAME}-lib
	${SRC}
	${PROTO_FILES}
)

target_include_directories(${PROJECT_NAME}-lib
	PRIVATE
		${TASK_THREAD_POOL_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME}-lib
	PUBLIC
		libpqxx::pqxx
		mimalloc-static
		unofficial::mongoose::mongoose
		yyjson::yyjson
)


add_executable(${PROJECT_NAME}
	${SRC}
	main.cpp
)

target_link_libraries(${PROJECT_NAME}
	PUBLIC ${PROJECT_NAME}-lib
)

target_link_options(${PROJECT_NAME}
	PRIVATE
		-static-libgcc
		-static-libstdc++
)
