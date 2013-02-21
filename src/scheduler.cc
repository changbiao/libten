#include "scheduler.hh"
#include "thread_context.hh"
#include <sys/syscall.h>

namespace ten {

scheduler::scheduler()
  : taskcount(0),
    canceled(false)
{
    update_cached_time();
}

scheduler::~scheduler() {
    if (getpid() == syscall(SYS_gettid)) {
        // if the main proc is exiting we need to cancel
        // all other procs (threads) and wait for them
        size_t nprocs = thread_context::count();
        this_ctx->cancel_all();
        while (thread_context::count()) {
            // TODO: remove this busy loop in favor of sleeping the proc
            std::this_thread::sleep_for(std::chrono::milliseconds{60});
        }

        // nasty hack for mysql thread cleanup
        // because it happens *after* all of my code, i have no way of waiting
        // for it to finish with an event (unless i joined all threads)
        unsigned sleep_ms = std::min((size_t)1500, nprocs*100);
        DVLOG(5) << "sleeping for " << sleep_ms << "ms to allow " << nprocs << " threads to cleanup";
        std::this_thread::sleep_for(std::chrono::milliseconds{sleep_ms});
        std::this_thread::yield();
    }
    // TODO: now that threads are remaining joinable
    // maybe shutdown can be cleaner...look into this
    // example: maybe if canceled we don't detatch
    // and we can join in the above loop instead of sleep loop
    //if (thread.joinable()) {
    //    thread.detach();
    //}
    // XXX: there is also a thing that will trigger a cond var
    // after thread has fully exited. that is another possiblity
    // once libstdc++ implements it
    runqueue.clear();
    // clean up system tasks
    while (!alltasks.empty()) {
        remove_task(ptr<task::pimpl>{alltasks.front().get()});
    }
    DVLOG(5) << "scheduler freed: " << this;
}

void scheduler::shutdown() {
    if (!_shutdown_sequence_initiated) {
        _shutdown_sequence_initiated = true;
        for (auto &t : alltasks) {
            if (t.get() == ctask.get()) continue; // don't add ourself to the runqueue
            t->cancel();
        }
    }
}

io& scheduler::get_io() {
    if (!_io) {
        _io.emplace();
    }
    return *_io;
}

void scheduler::loop() {
    DVLOG(5) << "entering loop";
    while (taskcount > 0) {
        schedule();
    }
    DVLOG(5) << "exiting loop";
}

void scheduler::check_canceled() {
    if (canceled) {
        shutdown();
    }
}

void scheduler::check_dirty_queue() {
    ptr<task::pimpl> t = nullptr;
    while (dirtyq.pop(t)) {
        DVLOG(5) << "dirty readying " << t;
        runqueue.push_front(t);
    }
}

void scheduler::check_timeout_tasks() {
    update_cached_time();
    // wake up sleeping tasks
    alarms.tick(now, [this](ptr<task::pimpl> t, std::exception_ptr exception) {
        if (exception != nullptr && t->exception == nullptr) {
            DVLOG(5) << "alarm with exception fired: " << t;
            t->exception = exception;
        }
        DVLOG(5) << "TIMEOUT on task: " << t;
        t->ready_for_io();
    });
}

void scheduler::wait(std::unique_lock <std::mutex> &lock, optional<proc_time_t> when) {
    // do not wait if runqueue is not empty
    check_dirty_queue();
    if (!runqueue.empty()) return;
    if (_io) {
        lock.unlock();
        _io->wait(when);
        lock.lock();
    } else {
        if (when) {
            _cv.wait_until(lock, *when);
        } else {
            _cv.wait(lock);
        }
    }
}

void scheduler::schedule() {
    try {
        do {
            check_canceled();
            check_dirty_queue();
            check_timeout_tasks();
            if (runqueue.empty()) {
                auto when = alarms.when();
                std::unique_lock<std::mutex> lock{_mutex};
                wait(lock, when);
            }
        } while (runqueue.empty());
        ptr<task::pimpl> t{runqueue.front()};
        runqueue.pop_front();
        ctask = t;
        DVLOG(5) << this << " swapping to: " << t;
        t->is_ready = false;
        ctx.swap(t->ctx, reinterpret_cast<intptr_t>(t.get()));
        ctask = nullptr;

        if (!t->fn) {
            remove_task(t);
        }
    } catch (backtrace_exception &e) {
        LOG(FATAL) << e.what() << "\n" << e.backtrace_str();
    } catch (std::exception &e) {
        LOG(FATAL) << e.what();
    }
}

void scheduler::attach_task(std::shared_ptr<task::pimpl> t) {
    ++taskcount;
    t->_scheduler.reset(this);
    alltasks.push_back(std::move(t));
}

void scheduler::remove_task(ptr<task::pimpl> t) {
    using namespace std;
    --taskcount;
    DCHECK(find(begin(runqueue), end(runqueue), t) == end(runqueue)) << "BUG: " << t
        << " found in runqueue while being deleted";
    auto i = find_if(begin(alltasks), end(alltasks), [=](shared_ptr<task::pimpl> &tt) -> bool {
        return tt.get() == t.get();
    });
    DCHECK(i != end(alltasks));
    alltasks.erase(i);
}



void scheduler::ready(ptr<task::pimpl> t, bool front) {
    DVLOG(5) << "readying: " << t;
    if (t->is_ready.exchange(true) == false) {
        if (this != &this_ctx->scheduler) {
            dirtyq.push(t);
            wakeup();
        } else {
            if (front) {
                runqueue.push_front(t);
            } else {
                runqueue.push_back(t);
            }
        }
    }
}

void scheduler::ready_for_io(ptr<task::pimpl> t) {
    DVLOG(5) << "readying for io: " << t;
    if (t->is_ready.exchange(true) == false) {
        runqueue.push_back(t);
    }
}

void scheduler::unsafe_ready(ptr<task::pimpl> t) {
    DVLOG(5) << "readying: " << t;
    runqueue.push_back(t);
}

bool scheduler::cancel_task_by_id(uint64_t id) {
    bool found = false;
    for (auto &t : alltasks) {
        if (t->id == id) {
            found = true;
            t->cancel();
            break;
        }
    }
    return found;
}

void scheduler::wakeup() {
    // TODO: speed this up?
    std::unique_lock<std::mutex> lock{_mutex};
    if (_io) {
        _io->wakeup();
    } else {
        _cv.notify_one();
    }
}

} // ten
