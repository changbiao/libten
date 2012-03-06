#define BOOST_TEST_MODULE ioproc test
#include <boost/test/unit_test.hpp>
#include "ioproc.hh"
#include "descriptors.hh"

using namespace ten;

const size_t default_stacksize=8*1024;

static void ioproc_sleeper() {
    ioproc io;
    int ret = iocall<int>(io, std::bind(usleep, 100));
    BOOST_CHECK_EQUAL(ret, 0);
}

BOOST_AUTO_TEST_CASE(ioproc_sleep_test) {
    procmain p;
    taskspawn(ioproc_sleeper);
    p.main();
}

static void dial_google() {
    socket_fd s(AF_INET, SOCK_STREAM);
    // getaddrinfo requires a rather large stack
    ioproc io(8*1024*1024);
    int status = iodial(io, s.fd, "www.google.com", 80);
    BOOST_CHECK_EQUAL(status, 0);
}

BOOST_AUTO_TEST_CASE(task_socket_dial) {
    procmain p;
    taskspawn(dial_google);
    p.main();
}

static void test_pool() {
    ioproc io(default_stacksize, 4);
    iochannel reply_chan;

    for (int i=0; i<4; ++i) {
        iocallasync<int>(io, std::bind(usleep, 100), reply_chan);
    }

    for (int i=0; i<4; ++i) {
        int r = iowait<int>(reply_chan);
        // usleep should return 0
        BOOST_CHECK_EQUAL(0, r);
    }
}

BOOST_AUTO_TEST_CASE(ioproc_thread_pool) {
    procmain p;
    taskspawn(test_pool);
    p.main();
}

static void fail() {
    throw std::runtime_error("fail has failed");
}

static void ioproc_failure() {
    ioproc io;
    std::string errmsg;
    try {
        iocall(io, fail);
        BOOST_CHECK(false);
    } catch (std::runtime_error &e) {
        errmsg = e.what();
    }
    BOOST_CHECK_EQUAL(errmsg, "fail has failed");
}

BOOST_AUTO_TEST_CASE(ioproc_error_test) {
    procmain p;
    taskspawn(ioproc_failure);
    p.main();
}
