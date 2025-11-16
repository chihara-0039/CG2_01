#pragma once
#include <string>

namespace StringUtility {
	//string‚ðwstring‚É•ÏŠ·‚·‚é
	std::wstring ConvertString(const std::string& str);

	//wstring‚ðstring‚É•ÏŠ·‚·‚é
	std::string ConvertString(const std::wstring& str);
};