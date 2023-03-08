#include "useful.hh"

std::string str_ipport(const sockaddr_in &addr)
{
    char buf[INET_ADDRSTRLEN];
    auto ptr = inet_ntop(AF_INET,&addr.sin_addr,buf,sizeof(buf));
    return (ptr != nullptr) ? (std::string(buf) + ":" + std::to_string(addr.sin_port)) : std::to_string(addr.sin_addr.s_addr);
}