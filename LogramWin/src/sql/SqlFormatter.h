#pragma once
#include <string>
#include <string_view>
#include <vector>

std::string SqlFormat(std::string_view sql);
std::vector<std::string> SqlTokenize(std::string_view sql);
