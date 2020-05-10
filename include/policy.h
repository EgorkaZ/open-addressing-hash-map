#pragma once

#include <cstddef>

struct LinearProbing {
    static size_t next(size_t curr, size_t, size_t size) {
        return (curr + 1) % size;
    }
};

struct QuadraticProbing {
    static size_t next(size_t curr, size_t step_num, size_t size) {
        return (curr + step_num * step_num) % size;
    }
};
