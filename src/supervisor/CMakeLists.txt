add_executable(superv
        Supervisor.hpp
        Supervisor.cpp)

target_link_libraries(superv
        json
        cxxopts
        yaml-cpp
        rt)