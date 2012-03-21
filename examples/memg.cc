#include "app.hh"
#include "net.hh"
#include "buffer.hh"
#include <unordered_map>
#include <boost/algorithm/string.hpp>

// poor-man's memcached
// http://www.darkcoding.net/software/in-memory-key-value-store-in-c-go-and-python/

using namespace ten;
const size_t default_stacksize=256*1024;

struct memg_config : app_config {
    std::string listen_addr;
    bool single;
};

static memg_config conf;

class memg_server : public netsock_server {
public:
    memg_server(size_t stacksize_=default_stacksize)
        : netsock_server("memg", stacksize_)
    {}

private:
    std::unordered_map<std::string, std::string> cache;

    void on_connection(netsock &s) {
        buffer buf(4*1024);
        std::stringstream ss;
        std::string line;
        ssize_t nw;
        std::string key;
        size_t value_length = 0;
        for (;;) {
            buf.reserve(4*1024);
            ssize_t nr = s.recv(buf.back(), buf.available());
            if (nr <= 0) goto done;
            buf.commit(nr);
            char *p = std::find(buf.front(), buf.back(), '\r');
            if (p == buf.back()) continue;
            std::stringstream os;
            std::string line(buf.front(), p - buf.front());
            buf.remove(line.size()+2);
            std::vector<std::string> parts;
            boost::split(parts, line, boost::is_any_of(" "));
            if (parts.empty()) continue;
            if (parts[0] == "get") {
                auto i = cache.find(parts[1]);
                if (i != cache.end()) {
                    os << "VALUE " << i->first << " 0 " << i->second.size() << "\r\n";
                    os << i->second << "\r\n";
                }
                os << "END\r\n";
                nw = s.send(os.str().data(), os.tellp());
            } else if (parts[0] == "set") {
                key = parts[1];
                value_length = boost::lexical_cast<uint32_t>(parts[4]);
                buf.reserve(value_length+2);
                while (buf.size() < value_length+2) {
                    nr = s.recv(buf.back(), buf.available());
                    if (nr <= 0) goto done;
                    buf.commit(nr);
                }
                std::string value(buf.front(), buf.front()+value_length);
                buf.remove(value_length+2);
                cache[key] = value;
                nw = s.send("STORED\r\n", 8);
            }
        }
done:
        if (conf.single) {
            procshutdown();
        }
    }
};

struct state : boost::noncopyable {
    application &app;
    memg_server server;

    state(application &app_) : app(app_) {}
};

static void startup(application &app) {
    taskname("startup");

    std::string addr = conf.listen_addr;
    uint16_t port = 11211;
    parse_host_port(addr, port);
    std::shared_ptr<state> st(std::make_shared<state>(app));
    st->server.serve(addr, port);
}

int main(int argc, char *argv[]) {
    application app("0.0.1", conf);
    namespace po = boost::program_options;
    app.opts.configuration.add_options()
        ("listen,L", po::value<std::string>(&conf.listen_addr)->default_value("127.0.0.1:11211"),
         "listen address:port")
        ("single", po::value<bool>(&conf.single)->zero_tokens(), "single connection")
    ;
    app.parse_args(argc, argv);
    taskspawn(std::bind(startup, std::ref(app)));
    return app.run();
}
