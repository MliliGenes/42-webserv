#pragma once

#ifndef CGIHANDLER_HPP
# define CGIHANDLER_HPP

# include <map>
# include <string>
# include <vector>

# include "Cgiparser.hpp"

struct cgirequest
{
	cgirequest();

	std::string method;
	std::string script_path;
	std::string interpreter_path;
	std::string query_string;
	std::string content_type;
	std::string body;
	std::string working_directory;
	std::map<std::string, std::string> extra_env;
};

class cgihandler
{
	public:
		cgihandler();

		// Runs the CGI process and parses its output into cgiresponse.
		bool execute(const cgirequest& request,
			cgiresponse& response,
			std::string& error,
			int timeout_seconds = 5) const;

	private:
		bool validate_request(const cgirequest& request, std::string& error) const;
		std::vector<std::string> build_environment(const cgirequest& request) const;
		bool run_process(const cgirequest& request,
			const std::vector<std::string>& environment,
			std::string& raw_output,
			std::string& error,
			int timeout_seconds) const;
};

#endif