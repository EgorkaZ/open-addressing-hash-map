#include "hash_set.h"
#include <unordered_set>

#include <iostream>

int main()
{
    std::unordered_set<int> s;
    s.insert(2);
    s.insert(2);
    auto borders = s.equal_range(2);
    for(auto it = borders.first; it != borders.second; ++it){
        std::cout << *it << '\n';
    }
    HashSet<int> set;
    while (std::cin) {
        int x;
        std::cin >> x;
        if (set.contains(x)) {
            std::cout << "*\n";
        }
        else {
            std::cout << "-\n";
        }
        set.insert(x);
    }
}
