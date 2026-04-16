#pragma once
#include <string>
#include <string_view>

// Substitute bind parameters (:1, :2 or ?) with actual values from cust1 JSON
std::string SqlParamSubstitute(std::string_view sql, std::string_view paramsJSON);
