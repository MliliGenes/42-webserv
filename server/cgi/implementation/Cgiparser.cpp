#include "../include/Cgiparser.hpp"

static std::string trim(const std::string& value)
{
	size_t begin = 0;
	while (begin < value.size()
		&& std::isspace(static_cast<unsigned char>(value[begin])))
		++begin;

	if (begin == value.size())
		return "";

	size_t end = value.size();
	while (end > begin
		&& std::isspace(static_cast<unsigned char>(value[end - 1])))
		--end;

	return value.substr(begin, end - begin);
}

static std::string to_lower(const std::string& value)
{
	std::string lowered;
	lowered.reserve(value.size());

	for (size_t i = 0; i < value.size(); ++i)
	{
		lowered.push_back(
			static_cast<char>(std::tolower(static_cast<unsigned char>(value[i]))));
	}

	return lowered;
}

static bool parse_status_from_http_line(const std::string& line, int& status_code)
{
	if (line.compare(0, 5, "HTTP/") != 0)
		return false;

	std::istringstream stream(line);
	std::string http_version;
	if (!(stream >> http_version >> status_code))
		return false;

	return (status_code >= 100 && status_code <= 599);
}

static bool parse_status_header_value(const std::string& value, int& status_code)
{
	std::istringstream stream(value);
	if (!(stream >> status_code))
		return false;
	return (status_code >= 100 && status_code <= 599);
}

static size_t find_header_separator(const std::string& raw_output, size_t& separator_length)
{
	size_t pos = raw_output.find("\r\n\r\n");
	if (pos != std::string::npos)
	{
		separator_length = 4;
		return pos;
	}

	pos = raw_output.find("\n\n");
	if (pos != std::string::npos)
	{
		separator_length = 2;
		return pos;
	}

	separator_length = 0;
	return std::string::npos;
}

cgiresponse::cgiresponse() : status_code(200) {}

cgiparser::cgiparser() {}

bool cgiparser::parse(const std::string& raw_output,
	cgiresponse& response,
	std::string& error) const
{
	try
	{
		response = cgiresponse();
		error.clear();

		if (raw_output.empty())
		{
			error = "CGI script returned an empty output.";
			return false;
		}
		size_t separator_length = 0;
		size_t separator_pos = find_header_separator(raw_output, separator_length);
		if (separator_pos == std::string::npos)
		{
			response.body = raw_output;
			return true;
		}
		std::string header_part = raw_output.substr(0, separator_pos);
		response.body = raw_output.substr(separator_pos + separator_length);

		std::istringstream stream(header_part);
		std::string line;
		while (std::getline(stream, line))
		{
			if (!line.empty() && line[line.size() - 1] == '\r')
			{
				line.erase(line.size() - 1);
			}

			line = trim(line);
			if (line.empty())
				continue;

			if (parse_status_from_http_line(line, response.status_code))
				continue;

			size_t colon_pos = line.find(':');
			if (colon_pos == std::string::npos)
				continue;

			std::string key = trim(line.substr(0, colon_pos));
			std::string value = trim(line.substr(colon_pos + 1));
			if (key.empty())
				continue;

			if (to_lower(key) == "status")
			{
				int parsed_status_code = 0;
				if (!parse_status_header_value(value, parsed_status_code))
				{
					error = "Invalid CGI Status header: " + value;
					return false;
				}
				response.status_code = parsed_status_code;
				continue;
			}

			response.headers[key] = value;
		}

		return true;
	}
	catch (const std::exception& e)
	{
		error = std::string("CGI parser exception: ") + e.what();
		return false;
	}
	catch (...)
	{
		error = "CGI parser exception: unknown failure.";
		return false;
	}
}
