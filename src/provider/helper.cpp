#include "helper.h"

#include <backward.hpp>
#include <iostream>
#include <sstream>
#include <unordered_map>

static std::unordered_map<void *, backward::StackTrace> map;

void register_alloc(void *ptr) {
    using namespace backward;
    StackTrace st;
    st.load_here(64);

    map[ptr] = st;
}
void register_free(void *ptr) { map.erase(ptr); }

void print_map() {
    for (auto &e : map) {
        using namespace backward;
        std::cout << e.first;

        auto st = e.second;

        std::stringstream s;

        Printer p;
        p.object = true;
        p.color_mode = ColorMode::always;
        p.address = true;
        p.print(st, s);

        std::cout << s.str() << std::endl;
    }
}