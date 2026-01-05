#include <iostream>
#include "utils.hpp"
#include <iostream>
#include "defer.cpp"

void report_error_location(const SourceSpan& span) {
    std::cerr << "Error between line " << span.start.line << ", column " << span.start.char_no 
    << " and line " << span.end.line << ", column " << span.end.char_no << std::endl;
}



std::string orig_name(const std::string& s) {
    size_t pos = s.find('.');
    return s.substr(0, pos);
}