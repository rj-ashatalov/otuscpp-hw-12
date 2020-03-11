#include <cstdlib>
#include <deque>
#include <iostream>
#include <list>
#include <memory>
#include <set>
#include <utility>
#include <boost/asio.hpp>
#include <utils/utils.h>
#include <async.h>
#include "remote/Message.cpp"


using boost::asio::ip::tcp;
using MessageQueue = std::deque<Remote::Message>;


class chat_participant
{
    public:
        virtual ~chat_participant()
        {
        }

        virtual void deliver(const Remote::Message& msg) = 0;
};

typedef std::shared_ptr<chat_participant> chat_participant_ptr;

class chat_room
{
    public:
        void join(chat_participant_ptr participant)
        {
            participants_.insert(participant);
            for (auto msg: recent_msgs_)
            {
                participant->deliver(msg);
            }
        }

        void leave(chat_participant_ptr participant)
        {
            participants_.erase(participant);
        }

        void deliver(const Remote::Message& msg)
        {
            recent_msgs_.push_back(msg);
            while (recent_msgs_.size() > max_recent_msgs)
            {
                recent_msgs_.pop_front();
            }

            for (auto participant: participants_)
            {
                participant->deliver(msg);
            }
        }

    private:
        std::set<chat_participant_ptr> participants_;
        enum
        {
            max_recent_msgs = 100
        };
        MessageQueue recent_msgs_;
};


class chat_session
        : public chat_participant, public std::enable_shared_from_this<chat_session>
{
    public:
        chat_session(tcp::socket socket, async::handle_t handler)
                : _socket(std::move(socket))
                , _handler(handler)
        {
        }

        void start()
        {
//            _handler->join(shared_from_this());
            do_read_header();
        }

        void deliver(const Remote::Message& msg)
        {
            bool write_in_progress = !_write_msgs.empty();
            _write_msgs.push_back(msg);
            if (!write_in_progress)
            {
                do_write();
            }
        }

    private:
        void do_read_header()
        {
            auto self(shared_from_this());
            boost::asio::async_read(_socket,
                    boost::asio::buffer(_read_msg.data(), Remote::Message::header_length),
                    [this, self](boost::system::error_code ec, std::size_t length)
                    {
                        if (!ec && _read_msg.decode_header())
                        {
                            do_read_body();
                        }
                        /*else
                        {
                            _handler->leave(shared_from_this());
                        }*/
                    });
        }

        void do_read_body()
        {
            auto self(shared_from_this());
            boost::asio::async_read(_socket,
                    boost::asio::buffer(_read_msg.body(), _read_msg.body_length()),
                    [this, self](boost::system::error_code ec, std::size_t length)
                    {
                        if (!ec)
                        {
                            _handler->deliver(_read_msg);
                            do_read_header();
                        }
                        else
                        {
                            _handler->leave(shared_from_this());
                        }
                    });
        }

        void do_write()
        {
            auto self(shared_from_this());
            boost::asio::async_write(_socket,
                    boost::asio::buffer(_write_msgs.front().data(),
                            _write_msgs.front().length()),
                    [this, self](boost::system::error_code ec, std::size_t length)
                    {
                        if (!ec)
                        {
                            _write_msgs.pop_front();
                            if (!_write_msgs.empty())
                            {
                                do_write();
                            }
                        }
                        else
                        {
                            _handler->leave(shared_from_this());
                        }
                    });
        }

        tcp::socket _socket;
        async::handle_t _handler;
        Remote::Message _read_msg;
        MessageQueue _write_msgs;
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
        tcp::endpoint endpoint(tcp::v4(), std::atoi(argv[0]));

        BulkServer bulkServer(io_service, std::move(endpoint), std::atoi(argv[1]));

        io_service.run();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}
