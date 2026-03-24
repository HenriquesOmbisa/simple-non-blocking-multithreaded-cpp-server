#include "EventLoop.hpp"
#include "Socket.hpp"

#include <sys/socket.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>

namespace mojoraw::core {

EventLoop::EventLoop(Router& r) : router_(r) {}

void EventLoop::init(int port, int threads) {
    port_ = port;
    if (threads <= 0)
        threads = static_cast<int>(std::thread::hardware_concurrency());
    if (threads < 1) threads = 1;
    numThreads_ = threads;

    std::cout << "[MojoRaw] port=" << port_
              << "  workers=" << numThreads_ << "\n";
}

void EventLoop::run() {
    std::vector<std::thread> workers;
    workers.reserve(static_cast<std::size_t>(numThreads_ - 1));

    for (int i = 1; i < numThreads_; ++i)
        workers.emplace_back([this]{ runWorker(); });

    runWorker(); // Main thread also runs a worker loop.

    for (auto& t : workers) t.join();
}

/* =================================================================
    Worker model: dedicated serverFd + epollFd via SO_REUSEPORT.
    Fully independent execution with zero cross-thread locks.
================================================================= */
void EventLoop::runWorker() {
    int serverFd = Socket::createServer(port_, /*reusePort=*/true);
    if (serverFd < 0) {
          std::cerr << "[MojoRaw] Failed to create listening socket\n";
        return;
    }

    int epollFd = epoll_create1(0);
    if (epollFd < 0) {
        perror("epoll_create1");
        close(serverFd);
        return;
    }

    epoll_event ev{};
    ev.events  = EPOLLIN | EPOLLET;
    ev.data.fd = serverFd;
    if (epoll_ctl(epollFd, EPOLL_CTL_ADD, serverFd, &ev) < 0) {
        perror("epoll_ctl serverFd");
        close(serverFd);
        close(epollFd);
        return;
    }

    std::unordered_map<int, Connection> conns;
    epoll_event events[MAX_EVENTS];

    while (true) {
        int nfds = epoll_wait(epollFd, events, MAX_EVENTS, EPOLL_TIMEOUT_MS);

        if (nfds < 0) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            break;
        }

        if (nfds == 0) {
            pruneIdleConnections(conns, epollFd);
            continue;
        }

        for (int i = 0; i < nfds; ++i) {
            int  fd   = events[i].data.fd;
            auto mask = events[i].events;

            if (fd == serverFd) {
                handleAccept(epollFd, serverFd, conns);
                continue;
            }

            if (mask & (EPOLLERR | EPOLLHUP)) {
                closeConn(fd, conns, epollFd);
                continue;
            }
            if (mask & EPOLLIN)  handleRead (fd, conns, epollFd);
            if (mask & EPOLLOUT) handleWrite(fd, conns, epollFd);
        }
    }

    close(epollFd);
    close(serverFd);
}

/* ---------------------------------------------------------------- */
void EventLoop::handleAccept(int epollFd, int serverFd,
                              std::unordered_map<int, Connection>& conns)
{
    while (true) {
        int clientFd = accept4(serverFd, nullptr, nullptr, SOCK_NONBLOCK);
        if (clientFd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            perror("accept4");
            break;
        }

        epoll_event ev{};
        ev.events  = EPOLLIN | EPOLLET;
        ev.data.fd = clientFd;

        if (epoll_ctl(epollFd, EPOLL_CTL_ADD, clientFd, &ev) < 0) {
            perror("epoll_ctl clientFd");
            close(clientFd);
            continue;
        }

        conns.emplace(clientFd, Connection{clientFd});
    }
}

/* ---------------------------------------------------------------- */
void EventLoop::handleRead(int fd,
                            std::unordered_map<int, Connection>& conns,
                            int epollFd)
{
    auto it = conns.find(fd);
    if (it == conns.end()) return;
    auto& conn = it->second;

    char buf[READ_BUFFER_SIZE];

    while (true) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n > 0) {
            conn.readBuffer.append(buf, static_cast<std::size_t>(n));
            conn.lastActivity = std::chrono::steady_clock::now();
            if (conn.readBuffer.size() > MAX_REQUEST_BYTES) {
                HttpResponse res;
                conn.keepAlive = false;
                conn.readBuffer.clear();
                conn.writeBuffer = res.status(413).send("Payload Too Large").toString(false);

                epoll_event ev{};
                ev.events  = EPOLLIN | EPOLLOUT | EPOLLET;
                ev.data.fd = fd;
                epoll_ctl(epollFd, EPOLL_CTL_MOD, fd, &ev);
                return;
            }
        } else if (n == 0) {
            closeConn(fd, conns, epollFd);
            return;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            closeConn(fd, conns, epollFd);
            return;
        }
    }

    // Wait for a complete HTTP request to avoid partial parsing.
    if (!HttpParser::isComplete(conn.readBuffer)) return;

    HttpRequest req;
    if (!HttpParser::parseRequest(conn.readBuffer, req)) {
        HttpResponse res;
        conn.writeBuffer = res.status(400).send("Bad Request").toString(false);
        conn.keepAlive   = false;
    } else {
        conn.keepAlive   = HttpParser::isKeepAlive(req);
        HttpResponse res = router_.route(req);
        conn.writeBuffer = res.toString(conn.keepAlive);
    }

    conn.readBuffer.clear();

    epoll_event ev{};
    ev.events  = EPOLLIN | EPOLLOUT | EPOLLET;
    ev.data.fd = fd;
    epoll_ctl(epollFd, EPOLL_CTL_MOD, fd, &ev);
}

/* ---------------------------------------------------------------- */
void EventLoop::handleWrite(int fd,
                             std::unordered_map<int, Connection>& conns,
                             int epollFd)
{
    auto it = conns.find(fd);
    if (it == conns.end()) return;
    auto& conn = it->second;

    while (!conn.writeBuffer.empty()) {
        ssize_t n = write(fd,
                          conn.writeBuffer.data(),
                          conn.writeBuffer.size());
        if (n > 0) {
            conn.writeBuffer.erase(0, static_cast<std::size_t>(n));
            conn.lastActivity = std::chrono::steady_clock::now();
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return;
            closeConn(fd, conns, epollFd);
            return;
        }
    }

    if (!conn.keepAlive) {
        closeConn(fd, conns, epollFd);
        return;
    }

    // Keep-alive path: switch back to read-only events.
    epoll_event ev{};
    ev.events  = EPOLLIN | EPOLLET;
    ev.data.fd = fd;
    epoll_ctl(epollFd, EPOLL_CTL_MOD, fd, &ev);
}

/* ---------------------------------------------------------------- */
void EventLoop::closeConn(int fd,
                           std::unordered_map<int, Connection>& conns,
                           int epollFd)
{
    epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, nullptr);
    close(fd);
    conns.erase(fd);
}

void EventLoop::pruneIdleConnections(std::unordered_map<int, Connection>& conns,
                                     int epollFd)
{
    const auto now = std::chrono::steady_clock::now();
    std::vector<int> stale;
    stale.reserve(conns.size());

    for (const auto& [fd, conn] : conns) {
        const auto idleMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - conn.lastActivity
        ).count();
        if (idleMs > IDLE_TIMEOUT_MS) stale.push_back(fd);
    }

    for (int fd : stale) closeConn(fd, conns, epollFd);
}

} // namespace mojoraw::core
