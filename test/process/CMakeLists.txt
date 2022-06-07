add_executable(process process.cpp)
target_link_libraries(process
        pthread
        ${Boost_LIBRARIES})