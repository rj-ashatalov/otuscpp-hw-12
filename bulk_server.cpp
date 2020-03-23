#include <cstdlib>
#include <deque>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>
#include <utils/utils.h>
#include <async.h>
#include "remote/Message.cpp"


using boost::asio::ip::tcp;
using MessageQueue = std::deque<Remote::Message>;


class chat_session : public std::enable_shared_from_this<chat_session>
{
    public:
        chat_session(tcp::socket socket, async::handle_t handler)
                : _socket(std::move(socket))
                , _handler(handler)
        {
        }

        void start()
        {
            do_read_data();
        }

    private:
        void do_read_data()
        {
            auto self(shared_from_this());
            boost::asio::async_read(_socket,
                    boost::asio::buffer(_read_msg.body(), Remote::Message::max_body_length),
                    [this, self](boost::system::error_code ec, std::size_t length)
                    {
                        {
                            std::unique_lock<std::mutex> locker(Utils::lockPrint);
                            std::cout << __PRETTY_FUNCTION__ << " read data: " << _read_msg.body() << "%end of message%"
                                      << std::endl;
                        }
                        if (length > 0)
                        {
                            async::receive(_handler, _read_msg.body(), length);
                            if (!ec)
                            {
                                do_read_data();
                            }
                            else
                            {
                                {
                                    std::unique_lock<std::mutex> locker(Utils::lockPrint);
                                    std::cout << "Disconnected: " << ec.message() << std::endl;
                                }
                                async::reset(_handler);
                            }
                        }
                        else if (ec)
                        {
                            {
                                std::unique_lock<std::mutex> locker(Utils::lockPrint);
                                std::cout << __PRETTY_FUNCTION__ << " error: " << ec.message() << std::endl;
                            }
                            async::reset(_handler);
                        }
                    });
        }

        tcp::socket _socket;
        async::handle_t _handler;
        Remote::Message _read_msg;
};


class BulkServer
{
    public:
        BulkServer(boost::asio::io_service& io_service, const tcp::endpoint& endpoint, int buffer)
                : _acceptor(io_service, endpoint)
                , _socket(io_service)
                , _handler(async::connect(buffer))
        {
            do_accept();
        }

        ~BulkServer()
        {
            {
                std::unique_lock<std::mutex> locker(Utils::lockPrint);
                std::cout << __PRETTY_FUNCTION__ << std::endl;
            }
            if (_handler)
            {
                async::disconnect(_handler);
            }
        }

    private:
        void do_accept()
        {
            _acceptor.async_accept(_socket,
                    [this](boost::system::error_code ec)
                    {
                        if (!ec)
                        {
                            std::unique_lock<std::mutex> locker(Utils::lockPrint);
                            std::cout << __PRETTY_FUNCTION__ << std::endl;
                            std::make_shared<chat_session>(std::move(_socket), _handler)->start();
                        }

                        do_accept();
                    });
        }

        tcp::acceptor _acceptor;
        tcp::socket _socket;
        async::handle_t _handler;
};


int main(int argc, char* argv[])
{
    try
    {
        if (argc < 3)
        {
            std::cerr << "Usage: bulk_server <port> <buffer>\n";
            return 1;
        }

        boost::asio::io_service io_service;
        tcp::endpoint endpoint(tcp::v4(), std::atoi(argv[1]));

        BulkServer bulkServer(io_service, std::move(endpoint), std::atoi(argv[2]));

        io_service.run();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    {
        std::unique_lock<std::mutex> locker(Utils::lockPrint);
        std::cout << "Stopping the bulk server" << std::endl;
    }
    return 0;
}
