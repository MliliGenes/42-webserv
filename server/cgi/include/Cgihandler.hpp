// anzid comments and explaination later

#pragma once

# include <map>
# include <string>
# include <vector>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <ctime>
#include <exception>
#include <fcntl.h>
#include <poll.h>
#include <sstream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

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

class scoped_sigpipe_ignore
{
	public:
		scoped_sigpipe_ignore() : enabled(false)
		{
			struct sigaction action;
			std::memset(&action, 0, sizeof(action));
			action.sa_handler = SIG_IGN;
			sigemptyset(&action.sa_mask);
			if (::sigaction(SIGPIPE, &action, &old_action) == 0)
				enabled = true;
		}

		~scoped_sigpipe_ignore()
		{
			if (enabled)
				::sigaction(SIGPIPE, &old_action, NULL);
		}

	private:
		bool enabled;
		struct sigaction old_action;
};
