#include <iostream>
#include <lutask/context/FiberContext.h>

namespace ctx = lutask::context;

int main() {
    ctx::FiberContext f1, f2, f3;
    f3 = ctx::FiberContext{ [&](ctx::FiberContext&& f)->ctx::FiberContext {
        f2 = std::move(f);
        for (;;) {
            std::cout << "f3\n";
            f2 = std::move(f1).Resume();
        }
        return {};
    } };
    f2 = ctx::FiberContext{ [&](ctx::FiberContext&& f)->ctx::FiberContext {
        f1 = std::move(f);
        for (;;) {
            std::cout << "f2\n";
            f1 = std::move(f3).Resume();
        }
        return {};
    } };
    f1 = ctx::FiberContext{ [&](ctx::FiberContext&& /*main*/)->ctx::FiberContext {
        for (;;) {
            std::cout << "f1\n";
            f3 = std::move(f2).Resume();
        }
        return {};
    } };
    std::move(f1).Resume();

    std::cout << "main: done" << std::endl;
    return EXIT_SUCCESS;
}