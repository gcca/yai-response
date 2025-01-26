find_package(Boost REQUIRED COMPONENTS system thread)
find_package(PostgreSQL REQUIRED)

add_library(yailib OBJECT yAI.cpp)
add_library(yAI::yAI ALIAS yailib)
target_include_directories(yailib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(yailib PUBLIC ${COMMON_COMPILE_OPTIONS})
target_link_libraries(yailib PUBLIC Boost::system Boost::thread)

add_library(yai-migration OBJECT yai-migration.cpp)
target_include_directories(yai-migration PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(yai-migration PUBLIC ${COMMON_COMPILE_OPTIONS})
target_link_libraries(yai-migration PUBLIC PostgreSQL::PostgreSQL)
