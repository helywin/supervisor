add_executable(supervd
        Supervisord.hpp
        Supervisord.cpp)


include(/home/jiang/code/third_party/thirdParty.cmake)

target_link_libraries(supervd
        Qt5::Core
        Qt5::Network
        json
        cxxopts
        )