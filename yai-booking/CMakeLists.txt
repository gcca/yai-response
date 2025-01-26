add_executable(yai-booking yai-booking.cc handlers/consultants-list.cpp handlers/import-csv.cc)
target_compile_options(yai-booking PUBLIC ${COMMON_COMPILE_OPTIONS})
target_link_libraries(yai-booking PUBLIC yAI::yAI PostgreSQL::PostgreSQL)

add_executable(yai-booking-migration yai-booking-migration.cc)
target_compile_options(yai-booking-migration PUBLIC ${COMMON_COMPILE_OPTIONS})
target_link_libraries(yai-booking-migration PUBLIC yai-migration
                                                   PostgreSQL::PostgreSQL)

add_abi(yai_booking_abi yai-booking-abi.cc)
