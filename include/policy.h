#pragma once

#include <vector>
#include <any>

struct LinearProbing {
    static size_t next(size_t curr, size_t step_num, size_t size){
        step_num--; // переменная нужна для общего интерфейса, но если её не использовать, компилятор будет ругаться
        return (curr + 1) % size;
    }
};
struct QuadraticProbing {
    static size_t next(size_t curr, size_t step_num, size_t size){
        return (curr + step_num * step_num) % size;
    }
};
