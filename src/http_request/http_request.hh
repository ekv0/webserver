#ifndef HTTP_REQUEST_HH
#define HTTP_REQUEST_HH

#include "scalable_buffer/scalable_buffer.hh"
#include "logger/logger.hh"

#include <string>
#include <unordered_map>
#include <regex>
#include <stdexcept>

class http_request
{
public:
    enum http_parse_state {
        REQUEST_LINE,
        HEADERS,
        BODY,
        FINISH,
        SYNTAX_ERROR
    };
    http_request();
    ~http_request();
    //clear internal buffers; ready for reuse in another parse
    void reset();
    //set appropriate internal vars and parse state
    http_parse_state parse(scalable_buffer &buf);
    //functions to retrieve parse results
    inline const std::string &method() const;
    inline const std::string &path() const;
    inline const std::string &params() const;
    inline const std::string &version() const;
    inline const std::unordered_map<std::string,std::string> &headers() const;
    inline const std::string &body() const;
    inline bool persistent() const;
    inline const int code() const;

private:
    http_parse_state state;
    std::string http_method, http_path, http_params, http_version;
    std::unordered_map<std::string,std::string> http_headers;
    std::string http_body;
    bool http_persistent;   //<side effect of http_headers
    int http_code;

    static const char *eol;  //end of line; \r\n

    bool parse_request_line(const std::string& line);
    void parse_headers(const std::string& line);
    void parse_body(const std::string& line);
};

const std::string &http_request::method() const
{
    return http_method;
}

const std::string &http_request::path() const
{
    return http_path;
}

const std::string &http_request::params() const
{
    return http_params;
}

const std::string &http_request::version() const
{
    return http_version;
}

const std::unordered_map<std::string,std::string> &http_request::headers() const
{
    return http_headers;
}

const std::string &http_request::body() const
{
    return http_body;
}

bool http_request::persistent() const
{
    return http_persistent;
}

const int http_request::code() const
{
    return http_code;
}

#endif //HTTP_REQUEST_HH