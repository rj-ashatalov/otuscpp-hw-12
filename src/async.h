#pragma once

#include <cstddef>
#include "Bulkmt.h"
#include <memory>

namespace async
{
    struct Worker;
    extern std::vector<std::shared_ptr<Worker>> _contextCache;

    using handle_t = void*;

    handle_t connect(std::size_t bulk);
    void receive(handle_t handle, const char* data, std::size_t size);
    void reset(handle_t handle);
    void disconnect(handle_t handle);

}

