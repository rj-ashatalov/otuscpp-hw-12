#pragma once

#include <vector>
#include <string>
#include <sstream>
#include <memory>
#include <algorithm>
#include <mutex>

namespace Utils
{
    extern std::mutex lockPrint;

    template<typename T>
    std::string ToString(T value)
    {
        return static_cast<std::string>(value);
    }

    template<typename T>
    std::string ToString(const std::shared_ptr<T>& value)
    {
        return static_cast<std::string>(*value);
    }
    //TODO @a.shatalov: implement string traits

    template<class T>
    std::string Join(std::vector<T> container, std::string&& delimiter)
    {
        if (container.size() <= 0)
        {
            return "";
        }

        std::stringstream output;
        std::for_each(container.begin(), std::prev(container.end()), [&output, &delimiter](auto& item)
        {
            output << Utils::ToString(item) << delimiter;
        });
        output << static_cast<std::string>(*container.back());
        return output.str();
    }

    size_t FibonacciNaive(const size_t value);
    size_t FactorialNaive(size_t value);
}