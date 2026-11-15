#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
class String {
 std::string value;
 public:
 String(const char *text=""):value(text){}
 String(const std::string &text):value(text){}
 String(int number):value(std::to_string(number)){}
 const char *c_str()const{return value.c_str();}
 String &operator+=(const String &other){value+=other.value;return *this;}
 friend String operator+(const String &a,const String &b){return a.value+b.value;}
};
