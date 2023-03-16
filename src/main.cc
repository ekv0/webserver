#include "webserver/webserver.hh"
int main()
{
    webserver w(
        65530,  //port
        true,   //listen
        true,   //client
        1024,   //max event
        128,    //backlog
        "/srv/www/html",
        {"index.html","index.htm","index.php"},
        1024,   //max connection
        1,  //accept thread
        120, //live time
        5,  //check interval
        true,   //enable logger
        logger::DEBUG,
        "/var/log/webserver.log",
        true,  //async
        1024,   //log queue cap
        12,  //nthread
        1024    //task queue cap
    );
    w.start();
}