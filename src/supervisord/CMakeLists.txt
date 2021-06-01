add_executable(supervd
        Supervisord.hpp
        Supervisord.cpp)


target_link_libraries(supervd
        Qt5::Core
        Qt5::Network
        json
        cxxopts
        )