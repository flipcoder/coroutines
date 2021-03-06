// Copyright (c) 2013 Maciej Gajewski
#include "coroutines/globals.hpp"
#include "coroutines/scheduler.hpp"

#include "profiling/profiling.hpp"

#include "coroutines_io/globals.hpp"
#include "coroutines_io/io_scheduler.hpp"
#include "coroutines_io/tcp_acceptor.hpp"

#include "client_connection.hpp"

#include <iostream>

#include <signal.h>

using namespace coroutines;
using namespace boost::asio::ip;

void handler(http_request const& req, http_response& res)
{
    res.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
    res.add("Content-Length", "14");
    res.add("Content-Type", "text/plain");

    res.stream() << "hello, world!\n";
}

void start_client_connection(tcp_socket& sock)
{
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
            //std::cout << "HTTP: connection accepted" << std::endl;
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
    CORO_PROF_DUMP();
    std::terminate();
}

int main(int argc, char** argv)
{
    signal(SIGINT, signal_handler);

    scheduler sched(4);
    io_scheduler io_sched(sched);
    set_scheduler(&sched);
    set_io_scheduler(&io_sched);

    io_sched.start();

    go("acceptor", server);

    sched.wait();
}
