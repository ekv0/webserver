#ifndef USEFUL_HH
#define USEFUL_HH

#include <arpa/inet.h>

#include <string>

std::string str_ipport(const sockaddr_in &addr);

#endif //USEFUL_HH