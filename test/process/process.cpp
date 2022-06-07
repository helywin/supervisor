/*********************************************************************************
 * FileName: process.cpp
 * Author: helywin <jiang770882022@hotmail.com>
 * Version: 0.0.1
 * Date: 2022-06-07 下午5:11
 * Description: 
 * Others:
*********************************************************************************/

#include <boost/process.hpp>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace bp = boost::process;

std::vector<std::string> read_outline(const std::string &file)
{
    bp::ipstream is;//reading pipe-stream
    bp::child c(bp::search_path("nm"), file, bp::std_out > is);

    std::vector<std::string> data;
    std::string line;

    std::cout << "read" << std::endl;
    while (c.running() && std::getline(is, line) && !line.empty())
        data.push_back(line);
    std::cout << "finish read" << std::endl;

    c.wait();
    std::cout << "finish wait" << std::endl;
    return data;
}

void start()
{
    bp::ipstream is;//reading pipe-stream
    std::string line;
    boost::asio::io_service ios;
    std::vector<char> buf;
    bp::async_pipe ap(ios);
    bp::child c("/data/supervd", bp::std_out > ap);
    ios.run();
    boost::asio::async_read(ap, boost::asio::buffer(buf),
                            [&buf](const boost::system::error_code &ec, std::size_t size) {
                                std::cout << buf.size() << std::endl;
                            });
    std::cout << "is running " << c.running() << std::endl;
}

int main()
{
    start();
    for (int i = 0; i < 100; ++i) {
        std::this_thread::sleep_for(1s);
        std::cout << "sleep" << std::endl;
    }
    //    c.wait();
}