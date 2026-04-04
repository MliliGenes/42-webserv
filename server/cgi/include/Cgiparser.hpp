#pragma once

# include <map>
# include <string>
#include <cctype>
#include <exception>
#include <sstream>

struct cgiresponse
{
	cgiresponse();

	int status_code;
	std::map<std::string, std::string> headers;
	std::string body;
};

class cgiparser
{
	public:
		cgiparser();

		// Parses raw CGI output into status code, headers, and body.
		bool parse(const std::string& raw_output,
			cgiresponse& response,
			std::string& error) const;
};
