#pragma once
#include "Arduino.h"
#include <utility>
#include <vector>
class WebServer {
 public:
 std::vector<std::pair<std::string,std::string>> fields;
 int args()const{return fields.size();}
 String argName(int i)const{return fields.at(i).first;}
 String arg(int i)const{return fields.at(i).second;}
};
