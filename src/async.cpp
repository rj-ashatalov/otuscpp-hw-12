#include <thread>
#include "async.h"
#include "Bulkmt.h"
#include "BulkImpl.h"
#include <array>

namespace async
{
    struct Worker
    {
            using Command = std::string;

            std::shared_ptr<Bulkmt> bulk;
            BulkImpl _bulkImpl;

        private:
            std::mutex _lockCommandLoop;
            std::queue<Command> _commandQueue;

        public:
            std::condition_variable checkCommandLoop;
            std::atomic_bool isDone = false;
            std::thread workerThread;

            std::atomic_bool isForceFinalize;

            Worker(const std::size_t& buffer)
                    : bulk(new Bulkmt(static_cast<int>(buffer)))
                    , _bulkImpl(bulk)
                    , _lockCommandLoop()
                    , workerThread([this]()
                    {
                        {
                            std::unique_lock<std::mutex> locker(Utils::lockPrint);
                            std::cout << std::this_thread::get_id() << " " << __PRETTY_FUNCTION__ << std::endl;
                        }
                        while (!isDone)
                        {
                            std::unique_lock<std::mutex> locker(_lockCommandLoop);
                            checkCommandLoop.wait(locker, [this]()
                            {
                                return !_commandQueue.empty() || isDone;
                            });

                            if (!_commandQueue.empty())
                            {
                                auto command = std::move(_commandQueue.front());
                                _commandQueue.pop();
                                locker.unlock();

                                bulk->ExecuteAll(command, command.size());
                                bulk->CleanOpenBlocks();

                                locker.lock();
                                if (isForceFinalize && _commandQueue.empty())
                                {
                                    isForceFinalize = false;
                                    bulk->Finalize();
                                }
                            }
                        }
                        bulk->Finalize();
                        _bulkImpl.Complete();
                    })
            {
            }

            void Add(const char* data, std::size_t size)
            {
                std::unique_lock<std::mutex> locker(_lockCommandLoop);
                if (isDone)
                {
                    return;
                }
                _commandQueue.emplace(data, size);
                checkCommandLoop.notify_one();
            }

            void Complete()
            {
                while (true)
                {
                    {
                        std::unique_lock<std::mutex> locker(_lockCommandLoop);
                        if (_commandQueue.empty())
                        {
                            break;
                        }
                    }
                    checkCommandLoop.notify_one();
                }

                isDone = true;
                checkCommandLoop.notify_one();
            }
    };

    std::vector<std::shared_ptr<Worker>> _contextCache;

    handle_t connect(std::size_t bulk)
    {
        _contextCache.emplace_back(new Worker(bulk));
        auto& ctx = _contextCache[_contextCache.size() - 1];
        return ctx.get();
    }

    void reset(handle_t handle)
    {
        if (handle == nullptr)
        {
            return;
        }

        auto worker = static_cast<Worker*>(handle);
        worker->isForceFinalize = true;
        worker->checkCommandLoop.notify_one();
    }

    void receive(handle_t handle, const char* data, std::size_t size)
    {
        if (handle == nullptr)
        {
            return;
        }

        static_cast<Worker*>(handle)->Add(data, size);
    }

    void disconnect(handle_t handle)
    {
        auto* worker = static_cast<Worker*>(handle);
        worker->Complete();
        worker->workerThread.join();

        _contextCache.erase(std::find_if(_contextCache.begin(), _contextCache.end(), [handle](auto&& item)
        {
            return item.get() == handle;
        }));
    }

}
