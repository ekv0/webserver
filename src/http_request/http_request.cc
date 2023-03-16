#include "http_request.hh"

using namespace std;

const char *http_request::eol = "\r\n";

http_request::http_request()
{
    reset();
}

http_request::~http_request()
{
}

void http_request::reset()
{
    http_headers.clear();
    state = REQUEST_LINE;
}

//usage:
//while ((state = req.parse(buf)) != http_request::FINISH && state != http_request::SYNTAX_ERROR) {
//    read into buf
//}
//http_reponse res(req,root);
//auto [base,len] = res.non_body();
//write base into fd

http_request::http_parse_state http_request::parse(scalable_buffer &buf)
{
    while (buf.readable() && state != FINISH) {
        auto end = search(buf.base(), buf.base() + buf.readable(), eol, eol + 2);
        std::string line(buf.base(), end);
        dbg(line,end - buf.base(),buf.readable());
        switch(state) {
            case REQUEST_LINE:
                if(!parse_request_line(line)) {
                    http_code = 400;
                    return SYNTAX_ERROR;   //syntax error
                }
                break;    
            case HEADERS:
                parse_headers(line);
                if(buf.readable() <= 2) {
                    state = FINISH;
                }
                break;
            case BODY:
                parse_body(line);
                break;
            default:
                break;
        }
        //eol not encountered; need to read more
        if(end == buf.base() + buf.readable()) {
            break;
        }
        //point to the next line
        buf.retrieved(end - buf.base() + 2);
    }
    if (state == FINISH) {
        http_code = 200;
    }
    return state;
}

bool http_request::parse_request_line(const std::string& line)
{
    regex patten("^([^ ]*) /([^ ?]*)(\\?[^ ]+)? HTTP/([^ ]*)$");
    smatch subMatch;
    if(!regex_match(line, subMatch, patten)) {           
        return false;
    }
    http_method = subMatch.str(1);
    http_path = subMatch.str(2);
    http_params = subMatch.str(3);
    http_version = subMatch.str(4);
    //set default connection behavior
    http_persistent = http_version == "1.1";
    
    state = HEADERS;
    return true;
}

void http_request::parse_headers(const std::string& line)
{
    regex patten("^([^:]*): ?(.*)$");
    smatch subMatch;
    if(regex_match(line, subMatch, patten)) {
        http_headers[subMatch[1]] = subMatch[2];
        if (subMatch[1] == "Connection") {
            http_persistent = subMatch[2] != "close";
        }
    }
    else {
        state = BODY;
    }
}

void http_request::parse_body(const std::string& line)
{
    http_body = line;
    state = FINISH;
}