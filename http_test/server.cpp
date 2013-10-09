// Copyright (c) 2013 Maciej Gajewski
#include "coroutines/globals.hpp"
#include "coroutines/scheduler.hpp"

#include "coroutines_io/globals.hpp"
#include "coroutines_io/service.hpp"
#include "coroutines_io/tcp_acceptor.hpp"
#include "coroutines_io/socket_streambuf.hpp"

#include "client_connection.hpp"

#include <iostream>
#include <array>
#include <ctime>

#include <signal.h>

using namespace coroutines;
using namespace boost::asio::ip;

void handler(http_request const& req, http_response& res)
{
    res.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
    res.add("Content-Length", "14");
    res.add("Content-Type", "text/plain");



    res.stream() << "hello, world!\n";

//    std::cout << "request served" << std::endl;
}

void start_client_connection(tcp_socket& sock)
{
//    std::cout << "client conencted" << std::endl;
    client_connection c(std::move(sock), handler);
    c.start();

}

void server()
{
    try
    {
        tcp_acceptor acc;
        acc.listen(tcp::endpoint(address_v4::any(), 8080));

        std::cout << "Server accepting connections" << std::endl;
        for(;;)
        {
            tcp_socket sock = acc.accept();
            go("client connection", start_client_connection, std::move(sock));
        }
    }
    catch(const std::exception& e)
    {
        std::cerr << "server error: " << e.what() << std::endl;
    }
}

void signal_handler(int)
{
    scheduler * sched = get_scheduler();
    if (sched)
    {
        sched->debug_dump();
    }
}

int main(int argc, char** argv)
{
    // install signal handler, for debugging
    signal(SIGINT, signal_handler);

    scheduler sched(1);
    service srv(sched);
    set_scheduler(&sched);
    set_service(&srv);

    srv.start();

    go("acceptor", server);


    sched.wait();
}
