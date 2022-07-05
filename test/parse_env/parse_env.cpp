/*********************************************************************************
 * FileName: parse_env.cpp
 * Author: helywin <jiang770882022@hotmail.com>
 * Version: 0.0.1
 * Date: 2022-07-05 下午1:40
 * Description: 
 * Others:
*********************************************************************************/

#include <regex>
#include <iostream>

#include <QDebug>
#include <QString>

QStringList parseEnv(const QString &str)
{
    // 替换环境变量
    QRegExp regExp(R"(\$\{.*?\})");
//    int pos = 0;    // where we are in the string
//    int count = 0;  // how many Eric and Eirik's we've counted
//    while (pos >= 0) {
//        pos = regExp.indexIn(str, pos);
//        if (pos >= 0) {
//            ++pos;      // move along in str
//            ++count;    // count our Eric or Eirik
//        }
//    }
    regExp.indexIn(str);
    return regExp.capturedTexts();
}

int main()
{
    std::regex regex(R"(\$\{(.*?)\})");
    std::string s(R"(${ROBOT_HOME}/conf/${DEPLOY_ID})");

    auto words_begin =
            std::sregex_iterator(s.begin(), s.end(), regex);
    auto words_end = std::sregex_iterator();

    std::cout << "Found "
              << std::distance(words_begin, words_end)
              << " words\n";

    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
        std::smatch match = *i;
        for (const auto & sm : match) {
            std::cout << " " << sm.str();
        }
    }
}