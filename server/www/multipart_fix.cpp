// ─────────────────────────────────────────────────────────────────────────────
// ADD THIS FUNCTION above handlePost in ResponseBuilder.cpp
//
// parseMultipart  –  extract the first file part from a multipart/form-data body.
//
// Browser uploads look like:
//
//   Content-Type: multipart/form-data; boundary=----WebKitFormBoundary7MA4YWxkTrZu0gW
//
//   ------WebKitFormBoundary7MA4YWxkTrZu0gW\r\n
//   Content-Disposition: form-data; name="file"; filename="photo.jpg"\r\n
//   Content-Type: image/jpeg\r\n
//   \r\n
//   <binary data>\r\n
//   ------WebKitFormBoundary7MA4YWxkTrZu0gW--\r\n
//
// Returns true and fills out_name / out_data on success.
// ─────────────────────────────────────────────────────────────────────────────

static bool parseMultipart(const std::string& body,
                            const std::string& content_type,
                            std::string&       out_name,
                            std::string&       out_data)
{
    // ── extract boundary from Content-Type header ────────────────────────────
    // e.g. "multipart/form-data; boundary=----WebKitFormBoundaryXXX"
    std::size_t bpos = content_type.find("boundary=");
    if (bpos == std::string::npos)
        return false;

    std::string boundary = "--" + content_type.substr(bpos + 9);
    // strip optional quotes around the boundary value
    if (!boundary.empty() && boundary[2] == '"')
    {
        boundary = "--" + content_type.substr(bpos + 10);
        std::size_t q = boundary.find('"');
        if (q != std::string::npos)
            boundary = boundary.substr(0, q);
    }
    // trim whitespace / CR
    while (!boundary.empty() &&
           (boundary[boundary.size()-1] == '\r' ||
            boundary[boundary.size()-1] == ' '))
        boundary.erase(boundary.size()-1);

    // ── find the first boundary line ─────────────────────────────────────────
    std::size_t part_start = body.find(boundary);
    if (part_start == std::string::npos)
        return false;
    part_start += boundary.size();

    // skip \r\n after boundary line
    if (part_start + 1 < body.size() &&
        body[part_start] == '\r' && body[part_start+1] == '\n')
        part_start += 2;
    else if (part_start < body.size() && body[part_start] == '\n')
        part_start += 1;
    else
        return false; // malformed

    // ── parse part headers until blank line ──────────────────────────────────
    std::string part_filename;
    std::size_t pos = part_start;

    while (pos < body.size())
    {
        std::size_t line_end = body.find("\r\n", pos);
        if (line_end == std::string::npos)
            line_end = body.find("\n", pos);
        if (line_end == std::string::npos)
            break;

        std::string line = body.substr(pos, line_end - pos);

        // blank line → end of part headers
        if (line.empty())
        {
            pos = line_end + (body[line_end] == '\r' ? 2 : 1);
            break;
        }

        // Content-Disposition: form-data; name="file"; filename="photo.jpg"
        std::string lower_line = line;
        for (std::size_t i = 0; i < lower_line.size(); ++i)
            lower_line[i] = static_cast<char>(
                std::tolower(static_cast<unsigned char>(lower_line[i])));

        if (lower_line.find("content-disposition") == 0)
        {
            std::size_t fn = line.find("filename=\"");
            if (fn != std::string::npos)
            {
                fn += 10; // skip filename="
                std::size_t fn_end = line.find("\"", fn);
                if (fn_end != std::string::npos)
                    part_filename = line.substr(fn, fn_end - fn);
            }
        }

        pos = line_end + (body[line_end] == '\r' ? 2 : 1);
    }

    if (part_filename.empty())
        return false; // no filename found — not a file part

    // ── extract part data up to the next boundary ────────────────────────────
    std::size_t data_start = pos;
    std::string closing = "\r\n" + boundary;
    std::size_t data_end = body.find(closing, data_start);
    if (data_end == std::string::npos)
    {
        // try without leading \r\n (some clients omit it)
        closing  = "\n" + boundary;
        data_end = body.find(closing, data_start);
        if (data_end == std::string::npos)
            return false;
    }

    out_name = part_filename;
    out_data = body.substr(data_start, data_end - data_start);
    return true;
}


// ─────────────────────────────────────────────────────────────────────────────
// REPLACE the upload section of handlePost (everything after the CGI block)
// with this version.  The CGI block above it stays exactly the same.
// ─────────────────────────────────────────────────────────────────────────────

/*  ... (CGI block unchanged) ...

    if (!route.uploadEnable)
        return buildError(403, config);
    if (route.uploadStore.empty())
        return buildError(500, config);

    std::string filename;
    std::string file_data;

    // ── try multipart/form-data (browser <input type="file">) ────────────────
    std::map<std::string, std::string>::const_iterator ct_it =
        req.headers.find("content-type");

    bool is_multipart = (ct_it != req.headers.end() &&
                         ct_it->second.find("multipart/form-data") != std::string::npos);

    if (is_multipart)
    {
        if (!parseMultipart(req.body, ct_it->second, filename, file_data))
            return buildError(400, config);
    }
    else
    {
        // ── plain POST body (curl --data, fetch with raw body) ───────────────
        file_data = req.body;

        // try Content-Disposition header for filename (curl path)
        std::map<std::string, std::string>::const_iterator cd_it =
            req.headers.find("content-disposition");
        if (cd_it != req.headers.end())
        {
            std::size_t fn = cd_it->second.find("filename=\"");
            if (fn != std::string::npos)
            {
                fn += 10;
                std::size_t fn_end = cd_it->second.find("\"", fn);
                if (fn_end != std::string::npos)
                    filename = cd_it->second.substr(fn, fn_end - fn);
            }
        }

        // fall back to timestamp name
        if (filename.empty())
        {
            std::ostringstream oss;
            oss << "upload_" << std::time(NULL);
            filename = oss.str();
        }
    }

    std::string save_path = resolve_path(route.uploadStore, filename);
    if (!writeFile(save_path, file_data))
        return buildError(500, config);

    Response res;
    res.body         = "<html><body>uploaded: " + filename + "</body></html>";
    res.status_code  = 201;
    res.status_message = statusMessage(201);
    res.headers["Content-Type"]   = "text/html";
    res.headers["Content-Length"] = sizeToString(res.body.size());
    res.headers["Location"]       = "/uploads/" + filename;
    return res;
*/
