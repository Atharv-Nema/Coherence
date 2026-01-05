#pragma once
#include "expr.hpp"

void report_error_location(const SourceSpan& span);
std::string orig_name(const std::string& s);