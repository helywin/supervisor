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

#include <nlohmann/json.hpp>
#include <QString>



void to_json(nlohmann::json& j, const QString& s) {
    j = s.toStdString();
}

void from_json(const nlohmann::json& j, QString& p) {
    p = QString::fromStdString(j.get<std::string>());
}

#endif//SUPERVISOR_QSTRINGJSON_HPP
