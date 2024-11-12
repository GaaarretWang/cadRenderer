#pragma once
#define CURL_STATICLIB
#include<string>
#include "util/fileUtil.h"

class Request {
public:
	Request() {};
	~Request() {};
public:
	// GET
	bool get(const std::string& url, std::string& response);

	// POST
	bool convertModelByPath(const std::string& url, const std::string& filename, const std::string& filepath, int precision, std::string& response);

	bool convertModelByFile(const std::string& url, const std::string& filename, const std::string& filepath, int precision, std::string& response);
};