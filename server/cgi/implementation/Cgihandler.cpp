#include "../include/Cgihandler.hpp"

static void close_fd(int& fd)
{
	if (fd >= 0)
	{
		::close(fd);
		fd = -1;
	}
}

static std::string size_to_string(size_t value)
{
	std::ostringstream stream;
	stream << value;
	return stream.str();
}

static std::string make_errno_message(const std::string& context)
{
	return context + ": " + std::string(std::strerror(errno));
}

static bool set_non_blocking(int fd)
{
	int flags = ::fcntl(fd, F_GETFL, 0);
	if (flags < 0)
	{
		return false;
	}

	if (::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
	{
		return false;
	}

	return true;
}

static bool is_timeout_exceeded(std::time_t start_time, int timeout_seconds)
{
	if (timeout_seconds <= 0)
	{
		return false;
	}

	return static_cast<int>(std::time(NULL) - start_time) > timeout_seconds;
}

cgirequest::cgirequest() : method("GET") {}

cgihandler::cgihandler() {}

bool cgihandler::execute(const cgirequest& request,
	cgiresponse& response,
	std::string& error,
	int timeout_seconds) const
{
	try
	{
		error.clear();
		response = cgiresponse();

		if (!validate_request(request, error))
		{
			return false;
		}

		std::string raw_output;
		std::vector<std::string> environment = build_environment(request);
		if (!run_process(request, environment, raw_output, error, timeout_seconds))
		{
			return false;
		}

		cgiparser parser;
		if (!parser.parse(raw_output, response, error))
		{
			return false;
		}

		return true;
	}
	catch (const std::exception& e)
	{
		error = std::string("CGI handler exception: ") + e.what();
		return false;
	}
	catch (...)
	{
		error = "CGI handler exception: unknown failure.";
		return false;
	}
}

bool cgihandler::validate_request(const cgirequest& request, std::string& error) const
{
	if (request.script_path.empty())
	{
		error = "CGI script path is empty.";
		return false;
	}

	if (request.interpreter_path.empty())
	{
		if (::access(request.script_path.c_str(), X_OK) != 0)
		{
			error = make_errno_message("CGI script is not executable");
			return false;
		}
	}
	else
	{
		if (::access(request.interpreter_path.c_str(), X_OK) != 0)
		{
			error = make_errno_message("CGI interpreter is not executable");
			return false;
		}

		if (::access(request.script_path.c_str(), R_OK) != 0)
		{
			error = make_errno_message("CGI script is not readable");
			return false;
		}
	}

	return true;
}

std::vector<std::string> cgihandler::build_environment(const cgirequest& request) const
{
	std::map<std::string, std::string> env;

	env["GATEWAY_INTERFACE"] = "CGI/1.1";
	env["SERVER_PROTOCOL"] = "HTTP/1.1";
	env["REQUEST_METHOD"] = request.method.empty() ? "GET" : request.method;
	env["SCRIPT_FILENAME"] = request.script_path;
	env["SCRIPT_NAME"] = request.script_path;
	env["QUERY_STRING"] = request.query_string;
	env["CONTENT_LENGTH"] = size_to_string(request.body.size());
	env["REDIRECT_STATUS"] = "200";

	if (!request.content_type.empty())
	{
		env["CONTENT_TYPE"] = request.content_type;
	}

	for (std::map<std::string, std::string>::const_iterator it = request.extra_env.begin();
		it != request.extra_env.end();
		++it)
	{
		env[it->first] = it->second;
	}

	std::vector<std::string> result;
	result.reserve(env.size());

	for (std::map<std::string, std::string>::const_iterator it = env.begin();
		it != env.end();
		++it)
	{
		result.push_back(it->first + "=" + it->second);
	}

	return result;
}

bool cgihandler::run_process(const cgirequest& request,
	const std::vector<std::string>& environment,
	std::string& raw_output,
	std::string& error,
	int timeout_seconds) const
{
	scoped_sigpipe_ignore sigpipe_guard;

	raw_output.clear();

	int stdin_pipe[2] = {-1, -1};
	int stdout_pipe[2] = {-1, -1};

	if (::pipe(stdin_pipe) != 0)
	{
		error = make_errno_message("Failed to create CGI stdin pipe");
		return false;
	}

	if (::pipe(stdout_pipe) != 0)
	{
		error = make_errno_message("Failed to create CGI stdout pipe");
		close_fd(stdin_pipe[0]);
		close_fd(stdin_pipe[1]);
		return false;
	}

	pid_t pid = ::fork();
	if (pid < 0)
	{
		error = make_errno_message("Failed to fork CGI process");
		close_fd(stdin_pipe[0]);
		close_fd(stdin_pipe[1]);
		close_fd(stdout_pipe[0]);
		close_fd(stdout_pipe[1]);
		return false;
	}

	if (pid == 0)
	{
		::dup2(stdin_pipe[0], STDIN_FILENO);
		::dup2(stdout_pipe[1], STDOUT_FILENO);

		close_fd(stdin_pipe[0]);
		close_fd(stdin_pipe[1]);
		close_fd(stdout_pipe[0]);
		close_fd(stdout_pipe[1]);

		if (!request.working_directory.empty())
		{
			::chdir(request.working_directory.c_str());
		}

		std::vector<char*> envp;
		envp.reserve(environment.size() + 1);
		for (size_t i = 0; i < environment.size(); ++i)
		{
			envp.push_back(const_cast<char*>(environment[i].c_str()));
		}
		envp.push_back(NULL);

		if (request.interpreter_path.empty())
		{
			char* argv[2];
			argv[0] = const_cast<char*>(request.script_path.c_str());
			argv[1] = NULL;
			::execve(request.script_path.c_str(), argv, &envp[0]);
		}
		else
		{
			char* argv[3];
			argv[0] = const_cast<char*>(request.interpreter_path.c_str());
			argv[1] = const_cast<char*>(request.script_path.c_str());
			argv[2] = NULL;
			::execve(request.interpreter_path.c_str(), argv, &envp[0]);
		}

		::_exit(127);
	}

	close_fd(stdin_pipe[0]);
	close_fd(stdout_pipe[1]);

	int write_fd = stdin_pipe[1];
	int read_fd = stdout_pipe[0];
	if (!set_non_blocking(write_fd) || !set_non_blocking(read_fd))
	{
		::kill(pid, SIGKILL);
		::waitpid(pid, NULL, 0);
		close_fd(write_fd);
		close_fd(read_fd);
		error = make_errno_message("Failed to set CGI pipes to non-blocking mode");
		return false;
	}

	const std::string& body = request.body;
	size_t body_offset = 0;
	std::time_t start_time = std::time(NULL);
	bool child_exited = false;
	int child_status = 0;

	while (true)
	{
		if (is_timeout_exceeded(start_time, timeout_seconds))
		{
			::kill(pid, SIGKILL);
			::waitpid(pid, &child_status, 0);
			close_fd(write_fd);
			close_fd(read_fd);
			error = "CGI execution timed out.";
			return false;
		}

		if (write_fd >= 0 && body_offset >= body.size())
		{
			close_fd(write_fd);
		}

		struct pollfd pollfds[2];
		nfds_t count = 0;
		int write_index = -1;
		int read_index = -1;

		if (write_fd >= 0)
		{
			write_index = static_cast<int>(count);
			pollfds[count].fd = write_fd;
			pollfds[count].events = POLLOUT;
			pollfds[count].revents = 0;
			++count;
		}

		if (read_fd >= 0)
		{
			read_index = static_cast<int>(count);
			pollfds[count].fd = read_fd;
			pollfds[count].events = POLLIN | POLLHUP;
			pollfds[count].revents = 0;
			++count;
		}

		if (count > 0)
		{
			int poll_result = ::poll(pollfds, count, 100);
			if (poll_result < 0)
			{
				if (errno != EINTR)
				{
					::kill(pid, SIGKILL);
					::waitpid(pid, &child_status, 0);
					close_fd(write_fd);
					close_fd(read_fd);
					error = make_errno_message("poll() failed while running CGI");
					return false;
				}
			}

			if (write_index >= 0 && (pollfds[write_index].revents & POLLOUT))
			{
				size_t to_write = body.size() - body_offset;
				if (to_write > 4096)
				{
					to_write = 4096;
				}

				ssize_t written = ::write(write_fd,
					body.c_str() + body_offset,
					to_write);
				if (written > 0)
				{
					body_offset += static_cast<size_t>(written);
				}
				else if (written < 0
					&& errno != EINTR
					&& errno != EAGAIN
					&& errno != EWOULDBLOCK)
				{
					::kill(pid, SIGKILL);
					::waitpid(pid, &child_status, 0);
					close_fd(write_fd);
					close_fd(read_fd);
					error = make_errno_message("Failed to write request body to CGI stdin");
					return false;
				}
			}

			if (write_index >= 0
				&& (pollfds[write_index].revents & (POLLERR | POLLHUP | POLLNVAL)))
			{
				close_fd(write_fd);
			}

			if (read_index >= 0 && (pollfds[read_index].revents & (POLLIN | POLLHUP)))
			{
				char buffer[4096];
				while (true)
				{
					ssize_t bytes = ::read(read_fd, buffer, sizeof(buffer));
					if (bytes > 0)
					{
						raw_output.append(buffer, static_cast<size_t>(bytes));
						continue;
					}

					if (bytes == 0)
					{
						close_fd(read_fd);
					}

					if (bytes < 0 && errno == EINTR)
					{
						continue;
					}

					if (bytes < 0
						&& (errno == EAGAIN || errno == EWOULDBLOCK))
					{
						break;
					}

					break;
				}
			}

			if (read_index >= 0 && (pollfds[read_index].revents & (POLLERR | POLLNVAL)))
			{
				::kill(pid, SIGKILL);
				::waitpid(pid, &child_status, 0);
				close_fd(write_fd);
				close_fd(read_fd);
				error = "Failed to read CGI stdout.";
				return false;
			}
		}

		if (!child_exited)
		{
			pid_t wait_result = ::waitpid(pid, &child_status, WNOHANG);
			if (wait_result == pid)
			{
				child_exited = true;
			}
			else if (wait_result < 0 && errno != EINTR)
			{
				close_fd(write_fd);
				close_fd(read_fd);
				error = make_errno_message("waitpid() failed while running CGI");
				return false;
			}
		}

		if (child_exited && read_fd < 0)
		{
			break;
		}
	}

	close_fd(write_fd);
	close_fd(read_fd);

	if (!child_exited)
	{
		if (::waitpid(pid, &child_status, 0) < 0)
		{
			error = make_errno_message("waitpid() failed after CGI process completion");
			return false;
		}
	}

	if (WIFSIGNALED(child_status))
	{
		error = "CGI process terminated by signal.";
		return false;
	}

	if (WIFEXITED(child_status)
		&& WEXITSTATUS(child_status) != 0
		&& raw_output.empty())
	{
		error = "CGI process exited with non-zero status and no output.";
		return false;
	}

	return true;
}
