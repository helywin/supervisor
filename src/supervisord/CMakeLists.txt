add_executable(supervd
        Supervisord.hpp
        Supervisord.cpp)

if (${CMAKE_ALONE})
    include(../../../third_party/thirdParty.cmake)
endif ()

target_link_libraries(supervd
        Qt5::Core
        Qt5::Network
        json
        cxxopts
        )