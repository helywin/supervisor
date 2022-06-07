/*********************************************************************************
 * FileName: QStringJson.hpp
 * Author: helywin <jiang770882022@hotmail.com>
 * Version: 0.0.1
 * Date: 2022-06-07 下午4:03
 * Description: 
 * Others:
*********************************************************************************/

#ifndef SUPERVISOR_QSTRINGJSON_HPP
#define SUPERVISOR_QSTRINGJSON_HPP

#include <QString>
#include <nlohmann/json.hpp>


void to_json(nlohmann::json &j, const QString &s)
{
    j = s.toStdString();
}

void from_json(const nlohmann::json &j, QString &p)
{
    p = QString::fromStdString(j.get<std::string>());
}

void to_json(nlohmann::json &j, const QStringList &list)
{
    std::for_each(list.begin(), list.end(), [&j](auto &v) { j.push_back(v); });
}

void from_json(const nlohmann::json &j, QStringList &list)
{
    std::for_each(j.begin(), j.end(), [&list](auto &v) { list.append(QString::fromStdString(v)); });
}

#endif//SUPERVISOR_QSTRINGJSON_HPP
