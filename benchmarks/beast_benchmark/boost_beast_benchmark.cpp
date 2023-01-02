//
// Copyright (c) 2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

//------------------------------------------------------------------------------
//
// Example: HTTP server, fast
//
//------------------------------------------------------------------------------

#include "fields_alloc.hpp"

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <list>
#include <memory>
#include <string>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

class http_worker
{
public:
    http_worker(http_worker const&) = delete;
    http_worker& operator=(http_worker const&) = delete;

    http_worker(tcp::acceptor& acceptor) :
            acceptor_(acceptor)
    {
    }

    void start()
    {
        accept();
        check_deadline();
    }

private:
    using alloc_t = fields_alloc<char>;
    //using request_body_t = http::basic_dynamic_body<beast::flat_static_buffer<1024 * 1024>>;
    using request_body_t = http::string_body;

    // The acceptor used to listen for incoming connections.
    tcp::acceptor& acceptor_;

    // The socket for the currently connected client.
    tcp::socket socket_{acceptor_.get_executor()};

    // The buffer for performing reads
    beast::flat_static_buffer<8192> buffer_;

    // The allocator used for the fields in the request and reply.
    alloc_t alloc_{8192};

    // The parser for reading the requests
    boost::optional<http::request_parser<request_body_t, alloc_t>> parser_;

    // The timer putting a time limit on requests.
    net::steady_timer request_deadline_{
            acceptor_.get_executor(), (std::chrono::steady_clock::time_point::max)()};

    // The string-based response message.
    boost::optional<http::response<http::string_body, http::basic_fields<alloc_t>>> string_response_;

    // The string-based response serializer.
    boost::optional<http::response_serializer<http::string_body, http::basic_fields<alloc_t>>> string_serializer_;

    void accept()
    {
        // Clean up any previous connection.
        {
            beast::error_code ec;
            socket_.close(ec);
            buffer_.consume(buffer_.size());
        }

        acceptor_.async_accept(
                socket_,
                [this](beast::error_code ec)
                {
                    if (ec)
                    {
                        accept();
                    }
                    else
                    {
                        // Request must be fully processed within 60 seconds.
                        request_deadline_.expires_after(
                                std::chrono::seconds(60));

                        read_request();
                    }
                });
    }

    void read_request()
    {
        // On each read the parser needs to be destroyed and
        // recreated. We store it in a boost::optional to
        // achieve that.
        //
        // Arguments passed to the parser constructor are
        // forwarded to the message object. A single argument
        // is forwarded to the body constructor.
        //
        // We construct the dynamic body with a 1MB limit
        // to prevent vulnerability to buffer attacks.
        //
        parser_.emplace(
                std::piecewise_construct,
                std::make_tuple(),
                std::make_tuple(alloc_));

        http::async_read(
                socket_,
                buffer_,
                *parser_,
                [this](beast::error_code ec, std::size_t)
                {
                    if (ec)
                        accept();
                    else
                        process_request(parser_->get());
                });
    }

    void process_request(http::request<request_body_t, http::basic_fields<alloc_t>> & req)
    {
        switch (req.method())
        {
            case http::verb::post:
                send_pong(req.body(), req.target());
                break;

            default:
                // We return responses indicating an error if
                // we do not recognize the request method.
                std::terminate();
        }
    }

    void send_pong(std::string &body, beast::string_view target)
    {
        // Request path must be absolute and not contain "..".
        if (target.empty() || target != "/ping")
        {
            std::terminate();
        }

        rapidjson::Document d;
        d.ParseInsitu(reinterpret_cast<char*>(body.data()));

        if(d.HasParseError() || !d.HasMember("sequence")) {
            std::terminate();
        }

        rapidjson::StringBuffer sb;
        rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

        writer.StartObject();

        writer.String("sequence");
        writer.Uint64(d["sequence"].GetUint64());

        writer.EndObject();

        string_response_.emplace(
                std::piecewise_construct,
                std::make_tuple(),
                std::make_tuple(alloc_));

        string_response_->result(http::status::ok);
        string_response_->keep_alive(true);
        string_response_->set(http::field::server, "Beast");
        string_response_->set(http::field::content_type, "application/json");
        string_response_->body() = sb.GetString();
        string_response_->prepare_payload();

        string_serializer_.emplace(*string_response_);

        http::async_write(
                socket_,
                *string_serializer_,
                [this](beast::error_code ec, std::size_t)
                {
                    socket_.shutdown(tcp::socket::shutdown_send, ec);
                    string_serializer_.reset();
                    string_response_.reset();
                    accept();
                });
    }

    void check_deadline()
    {
        // The deadline may have moved, so check it has really passed.
        if (request_deadline_.expiry() <= std::chrono::steady_clock::now())
        {
            // Close socket to cancel any outstanding operation.
            socket_.close();

            // Sleep indefinitely until we're given a new deadline.
            request_deadline_.expires_at(
                    (std::chrono::steady_clock::time_point::max)());
        }

        request_deadline_.async_wait(
                [this](beast::error_code)
                {
                    check_deadline();
                });
    }
};

int main(int argc, char* argv[])
{
    try
    {
        // Check command line arguments.
        if (argc != 5)
        {
            std::cerr << "Usage: http_server_fast <address> <port> <num_workers> {spin|block}\n";
            std::cerr << "  For IPv4, try:\n";
            std::cerr << "    http_server_fast 0.0.0.0 80 . 100 block\n";
            std::cerr << "  For IPv6, try:\n";
            std::cerr << "    http_server_fast 0::0 80 . 100 block\n";
            return EXIT_FAILURE;
        }

        auto const address = net::ip::make_address(argv[1]);
        unsigned short port = static_cast<unsigned short>(std::atoi(argv[2]));
        int num_workers = std::atoi(argv[3]);
        bool spin = (std::strcmp(argv[4], "spin") == 0);

        net::io_context ioc{1};
        tcp::acceptor acceptor{ioc, {address, port}};

        std::list<http_worker> workers;
        for (int i = 0; i < num_workers; ++i)
        {
            workers.emplace_back(acceptor);
            workers.back().start();
        }

        if (spin)
            for (;;) ioc.poll();
        else
            ioc.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
