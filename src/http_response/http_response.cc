#include "http_response.hh"

using namespace std;

const char *http_response::eol = "\r\n";

const unordered_map<int, string> http_response::desc = {
    {200, "OK"},
    {400, "Bad Request"},
    {401, "Unauthorized"},
    {403, "Forbidden"},
    {404, "Not Found"},
    {500, "Internal Server Error"},
    {503, "Service Unavailable"}
};

const std::set<std::string> http_response::default_index_pages = {
    "index.html",
    "index.htm",
    "index.php"
};

void http_response::init(const http_request &req,scalable_buffer &buf,std::string root,const std::set<std::string> &index_pages)
{
    this->index_pages = index_pages;
    http_code = req.code();
    http_path = req.path();
    http_version = req.version().empty() ? "1.1" : req.version();    //when syntax error, respond with version 1.1
    http_persistent = req.persistent();

    buf.clear();
    if (root.back() != '/') {   //root folder not ended with '/'
        root.push_back('/');
    }
    //deal with http code
    if (http_code == 200) {
        if (http_path != "") {
            file_path = root + http_path;
            if (stat(file_path.c_str(),&fstat) < 0 || S_ISDIR(fstat.st_mode)) {
                http_code = 404;
            }
        }
        //root; try with every default index page
        else {
            http_code = 404;
            for (const auto &page : default_index_pages) {
                file_path = root + page;
                if (stat(file_path.c_str(),&fstat) == 0 && !S_ISDIR(fstat.st_mode)) {
                    http_code = 200;
                    break;
                }
            }
        }
    }
    //status line
    make_status_line(buf);
    //map body and set content-length
    map_body();
    //header lines
    make_header_lines(buf);
    //blank line separating body and non-body parts
    buf.append(eol);
}

void http_response::init(int code,scalable_buffer &buf)
{
    http_code = code;

    if (file && munmap(file,content_len) != 0) {
        log_err("unmap failed!");
    }
    file = nullptr;
    buf.clear();
    //init
    http_version = "1.1";
    http_persistent = false;
    //make response
    make_status_line(buf);
    map_body();
    make_header_lines(buf);
    buf.append(eol);
}

http_response::~http_response()
{
    if (file && munmap(file,content_len) != 0) {
        log_err("unmap failed!");
    }
}

void http_response::make_status_line(scalable_buffer &buf)
{
    buf.append("HTTP/");
    buf.append(http_version);
    buf.append(" ");
    buf.append(to_string(http_code));
    buf.append(" ");
    buf.append(desc.at(http_code));
    buf.append(eol);
}

void http_response::make_header_lines(scalable_buffer &buf)
{
    //Date
    buf.append("Date: ");
    buf.append(gmt_time());
    buf.append(eol);
    //Connection
    buf.append("Connection: ");
    if (http_persistent) {
        buf.append("keep-alive");
        buf.append(eol);
        buf.append("keep-alive: max=6, timeout=120");
    }
    else {
        buf.append("close");
    }
    buf.append(eol);
    //Content-type
    buf.append("Content-type: ");
    buf.append(file_type());
    buf.append(eol);
    //Content-Length
    buf.append("Content-Length: ");
    buf.append(to_string(content_len));
    buf.append(eol);
}

void http_response::map_body()
{
    if (http_code == 200) {
        content_len = fstat.st_size;
        int fd = open(file_path.c_str(),O_RDONLY);
        if (fd < 0) {
            log_err("response file open error");
            file = nullptr;
            content_len = 0;
            return;
            // throw runtime_error("response file open error");
        }
        if ((file = static_cast<char *>(mmap(0,content_len,PROT_READ,MAP_PRIVATE,fd,0))) == MAP_FAILED) {
            file = nullptr;
            content_len = 0;
            log_err("file map failed");
        }
        close(fd);
    }
    else {
        auto body = err_msg();
        content_len = body.size();
        if ((file = static_cast<char *>(mmap(0,content_len,PROT_READ | PROT_WRITE,MAP_PRIVATE | MAP_ANONYMOUS,-1,0))) == MAP_FAILED) {
            file = nullptr;
            content_len = 0;
            log_err("response mapping for error message failed");
            return;
        }
        memcpy(file,body.c_str(),content_len);
    }
}

string http_response::err_msg()
{
    auto mess = to_string(http_code) + " " + desc.at(http_code);
    return "<html><head><title>" +
        mess +
        "</title></head><body><center><h1>" +
        mess + 
        "</h1></center><hr><center><a href=\"https://github.com/ekv0/\">ekv0</a>'s <a href=\"https://github.com/ekv0/webserver\">webserver</a>/0.0.1</center></body></html>\r\n";
}

std::pair<char *,size_t> http_response::body() const
{
    return {file,content_len};
}

std::string http_response::gmt_time()
{
    auto now = time(nullptr);
    auto tm = gmtime(&now);
    char buf[64];
    strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S %Z", tm);
    return string(buf);
}

const unordered_map<string, string> http_response::suffix_type = {
    {".html", "text/html"},
    {".htm", "text/html"},
    {".xml", "text/xml"},
    {".xhtml", "application/xhtml+xml"},
    {".txt", "text/plain"},
    {".rtf", "application/rtf"},
    {".pdf", "application/pdf"},
    {".word", "application/nsword"},
    {".png", "image/png"},
    {".gif", "image/gif"},
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".au", "audio/basic"},
    {".mpeg", "video/mpeg"},
    {".mpg", "video/mpeg"},
    {".avi", "video/x-msvideo"},
    {".gz", "application/x-gzip"},
    {".tar", "application/x-tar"},
    {".css", "text/css"},
    {".js", "text/javascript"},
};

std::string http_response::file_type()
{
    if (http_code != 200) {
        return suffix_type.at(".html");
    }
    auto pos = file_path.find_last_of('.');
    if(pos == string::npos) {
        return "text/plain";
    }
    auto suffix = file_path.substr(pos);
    if(suffix_type.count(suffix) == 1) {
        return suffix_type.at(suffix);
    }
    return "text/plain";
}