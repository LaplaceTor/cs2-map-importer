#pragma once

#include <cstddef>

namespace stdext {

template<typename T>
using checked_array_iterator = T;

template<typename T>
using unchecked_array_iterator = T;

template<typename T>
constexpr T make_unchecked_array_iterator(T iter) {
    return iter;
}

template<typename T>
constexpr T make_checked_array_iterator(T iter, std::size_t /*count*/, std::size_t index = 0) {
    return iter + index;
}

} // namespace stdext
