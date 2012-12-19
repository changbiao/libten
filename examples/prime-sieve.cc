#include "ten/channel.hh"
#include "ten/task/compat.hh"
#include <iostream>

using namespace ten;
using namespace ten::compat;

// adapted from http://golang.org/doc/go_tutorial.html#tmp_360

void generate(channel<int> out) {
    for (int i=2; ; ++i) {
        out.send(std::move(i));
    }
}

void filter(channel<int> in, channel<int> out, int prime) {
    for (;;) {
        int i = in.recv();
        if (i % prime != 0) {
            out.send(std::move(i));
        }
    }
}

void primes() {
    channel<int> ch;
    taskspawn(std::bind(generate, ch));
    for (int i=0; i<100; ++i) {
        int prime = ch.recv();
        std::cout << prime << "\n";
        channel<int> out;
        taskspawn(std::bind(filter, ch, out, prime));
        ch = out;
    }
    procshutdown();
}

int main(int argc, char *argv[]) {
    procmain p;
    taskspawn(primes);
    return p.main(argc, argv);
}
