# Fuzz targets for chatterino2, linked like upstream's tests (tests/CMakeLists.txt).
# MAYHEM_FUZZ_MAIN is the object/flag providing main(): -fsanitize=fuzzer (libFuzzer) or the
# standalone driver object. Each harness is built twice: <name> and <name>-standalone.
set(MAYHEM_HARNESS_DIR "${CMAKE_CURRENT_LIST_DIR}/harnesses")

function(mayhem_fuzz_target name src)
    foreach(kind fuzz standalone)
        if(kind STREQUAL "fuzz")
            set(tgt ${name})
        else()
            set(tgt ${name}-standalone)
        endif()
        add_executable(${tgt} EXCLUDE_FROM_ALL "${MAYHEM_HARNESS_DIR}/${src}")
        target_link_libraries(${tgt} PRIVATE ${ARGN} "${MAYHEM_LSAN_OFF_OBJ}")
        if(kind STREQUAL "fuzz")
            target_link_options(${tgt} PRIVATE "$ENV{LIB_FUZZING_ENGINE}")
        else()
            target_link_libraries(${tgt} PRIVATE "${MAYHEM_STANDALONE_OBJ}")
        endif()
        set_target_properties(${tgt} PROPERTIES AUTOMOC OFF AUTORCC OFF
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/fuzz")
    endforeach()
endfunction()

mayhem_fuzz_target(fuzz_irc_message fuzz_irc_message.cpp chatterino-lib chatterino-mocks)
mayhem_fuzz_target(fuzz_eventsub_message fuzz_eventsub_message.cpp twitch-eventsub-ws)
mayhem_fuzz_target(fuzz_link_parser fuzz_link_parser.cpp chatterino-lib)
mayhem_fuzz_target(fuzz_filter fuzz_filter.cpp chatterino-lib chatterino-mocks)
mayhem_fuzz_target(fuzz_seventv_eventapi fuzz_seventv_eventapi.cpp chatterino-lib)
mayhem_fuzz_target(fuzz_pubsub_message fuzz_pubsub_message.cpp chatterino-lib)
mayhem_fuzz_target(fuzz_bttv_liveupdate fuzz_bttv_liveupdate.cpp chatterino-lib)
mayhem_fuzz_target(fuzz_recent_messages fuzz_recent_messages.cpp chatterino-lib chatterino-mocks)
