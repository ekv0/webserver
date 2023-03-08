#ifndef HTTP_RESONSE_HH
#define HTTP_RESONSE_HH

#include "scalable_buffer/scalable_buffer.hh"
#include "logger/logger.hh"
#include "http_request/http_request.hh"

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <string>
#include <ctime>
#include <set>  //ordered set to get the first matched default index page
#include <unordered_map>
#include <utility>

class http_response
{
public:
    http_response() = default;
    void init(const http_request &req,scalable_buffer &buf,std::string root,const std::set<std::string> &index_pages = default_index_pages);
    void init(const http_request &req,scalable_buffer &buf,std::string root,std::set<std::string> &&index_pages) {
        init(req,buf,root,index_pages);
    }
    //generate error http response by http code
    void init(int code,scalable_buffer &buf);
    ~http_response();
    //address and length of body content
    std::pair<char *,size_t> body() const;

private:
    static const std::unordered_map<int,std::string> desc;
    static const std::unordered_map<std::string,std::string> suffix_type;
    static const std::set<std::string> default_index_pages; //<default index pages; will be looked up in order
    static const char *eol;  //end of line; \r\n

    std::set<std::string> index_pages;  //<index pages

    //from request
    int http_code;
    std::string http_path;
    std::string http_version;
    bool http_persistent;
    //generate for response
    std::string file_path;
    char *file = nullptr;
    struct stat fstat;
    size_t content_len;

    void make_status_line(scalable_buffer &buf);
    void make_header_lines(scalable_buffer &buf);
    void map_body();
    std::string err_msg();

    std::string gmt_time();
    std::string file_type();
};

#endif //HTTP_RESONSE_HH