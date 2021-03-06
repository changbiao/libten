#include <thread>
#include "ten/thread_guard.hh"
#include "ten/task/rendez.hh"
#include "ten/logging.hh"

using namespace ten;

struct state {
    qutex q;
    rendez r;
    int x;

    state() : x(0) {}
};

void qlocker(std::shared_ptr<state> st) {
    taskname("qlocker");
    taskstate("looping");
    for (int i=0; i<1000; ++i) {
        std::lock_guard<qutex> lk(st->q);
        ++(st->x);
    }
    taskstate("done");
    std::lock_guard<qutex> lk(st->q);
    taskname("qlocker %d", st->x);
    st->r.wakeup();
}

bool is_done(int &x) {
    taskstate("sleeping on rendez: %d %x", x, &x);
    return x == 20*1000;
}

void qutex_task_spawn() {
    taskname("qutex_task_spawn");
    std::shared_ptr<state> st = std::make_shared<state>();
    std::vector<thread_guard> threads;
    for (int i=0; i<20; ++i) {
        taskstate("spawning %d", i);
        threads.emplace_back(task::spawn_thread([=] {
            qlocker(st);
        }));
    }
    std::unique_lock<qutex> lk(st->q);
    st->r.sleep(lk, std::bind(is_done, std::ref(st->x)));
    CHECK(st->x == 20*1000);
}

int main() {
    return task::main([] {
        for (int i=0; i<10; ++i) {
            task::spawn(qutex_task_spawn);
        }
    });
}

