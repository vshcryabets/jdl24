#include "SourceLinuxImpl.h"

#if defined(__linux__)

#include <cerrno>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <utility>

#include <fcntl.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <termios.h>
#include <unistd.h>

namespace dl24 {

SourceLinuxImpl::SourceLinuxImpl(std::string device)
    : device_(std::move(device)) {}

SourceLinuxImpl::~SourceLinuxImpl() {
    close();
}

Error SourceLinuxImpl::open(const UartConfig& config) {
    if (isOpen()) {
        return Error(ErrorCode::AlreadyOpen, "Port already open");
    }

    unsigned baud = 0;
    if (!toBaudConstant(config.baudRate, baud)) {
        return Error(ErrorCode::InvalidParameter, "Invalid UART baudrate configuration");
    }

    fd_ = ::open(device_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) {
        return Error(ErrorCode::OpenFailed, "Failed to open UART port");
    }

    if (!configurePort(config)) {
        ::close(fd_);
        fd_ = -1;
        return Error(ErrorCode::OpenFailed, "Failed to configure UART port");
    }

    // eventfd lets close() wake the worker out of a blocked poll() at once.
    wakeFd_ = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (wakeFd_ < 0) {
        ::close(fd_);
        fd_ = -1;
        return Error(ErrorCode::OpenFailed, "Failed to create eventfd");
    }

    running_.store(true);
    worker_ = std::thread(&SourceLinuxImpl::workerLoop, this);
    return Error::None;
}

Error SourceLinuxImpl::close() {
    if (!running_.load() && fd_ < 0) {
        return Error::None;
    }

    running_.store(false);

    // Wake the worker's poll() so it can observe running_ == false.
    if (wakeFd_ >= 0) {
        const uint64_t one = 1;
        ssize_t rc = ::write(wakeFd_, &one, sizeof(one));
        (void) rc;  // best-effort; EAGAIN only if the counter is saturated
    }

    if (worker_.joinable()) {
        worker_.join();
    }

    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    if (wakeFd_ >= 0) {
        ::close(wakeFd_);
        wakeFd_ = -1;
    }
    return Error::None;
}

bool SourceLinuxImpl::isOpen() const {
    return fd_ >= 0;
}

Error SourceLinuxImpl::write(const uint8_t* data, BufferSize_t size) {
    if (!isOpen()) {
        return Error(ErrorCode::NotOpen, "UART port not open");
    }
    size_t total = 0;
    
    while (total < size) {
        ssize_t n = ::write(fd_, data + total, size - total);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Output buffer full; wait until the fd is writable again.
                pollfd pfd{fd_, POLLOUT, 0};
                if (::poll(&pfd, 1, -1) < 0 && errno != EINTR) {
                    return Error(ErrorCode::WriteFailed, "Failed to write to UART port");
                }
                continue;
            }
            return Error(ErrorCode::WriteFailed, "Failed to write to UART port");
        }
        total += static_cast<size_t>(n);
    }
    return Error::None;
}

void SourceLinuxImpl::setListener(Listener* listener) {
    listener_.store(listener);
}

void SourceLinuxImpl::workerLoop() {
    uint8_t buffer[256];
    pollfd fds[2];
    fds[0] = {fd_, POLLIN, 0};      // serial data
    fds[1] = {wakeFd_, POLLIN, 0};  // shutdown signal

    while (running_.load()) {
        int rc = ::poll(fds, 2, -1);
        if (rc < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }

        // Shutdown requested.
        if (fds[1].revents & POLLIN) {
            break;
        }

        if (fds[0].revents & POLLIN) {
            ssize_t n = ::read(fd_, buffer, sizeof(buffer));
            if (n > 0) {
                // std::cout << "Received " << n << " bytes: ";
                // // Debug: dump data to stdout in hex
                // for (BufferSize_t i = 0; i < n; ++i) {
                //     std::cout << std::hex << std::setw(2) << std::setfill('0') 
                //             << static_cast<int>(buffer[i]) << " ";
                // }
                // std::cout << std::dec << std::endl;
                
                Listener* l = listener_.load();
                if (l != nullptr) {
                    l->onDataReceived(buffer, static_cast<BufferSize_t>(n));
                }
            } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK &&
                       errno != EINTR) {
                break;
            }
        }

        // Device went away.
        if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
            break;
        }
    }
}

bool SourceLinuxImpl::configurePort(const UartConfig& config) {
    termios tty{};
    if (::tcgetattr(fd_, &tty) != 0) {
        return false;
    }

    // Raw mode: no canonical processing, echo, signals, or byte translation.
    cfmakeraw(&tty);

    unsigned baud = 0;
    toBaudConstant(config.baudRate, baud);  // validated by caller
    cfsetispeed(&tty, baud);
    cfsetospeed(&tty, baud);

    // Data bits.
    tty.c_cflag &= ~CSIZE;
    switch (config.dataBits) {
        case DataBits::Five:  tty.c_cflag |= CS5; break;
        case DataBits::Six:   tty.c_cflag |= CS6; break;
        case DataBits::Seven: tty.c_cflag |= CS7; break;
        case DataBits::Eight: tty.c_cflag |= CS8; break;
    }

    // Parity.
    switch (config.parity) {
        case Parity::None:
            tty.c_cflag &= ~PARENB;
            break;
        case Parity::Even:
            tty.c_cflag |= PARENB;
            tty.c_cflag &= ~PARODD;
            break;
        case Parity::Odd:
            tty.c_cflag |= PARENB;
            tty.c_cflag |= PARODD;
            break;
    }

    // Stop bits.
    if (config.stopBits == StopBits::Two) {
        tty.c_cflag |= CSTOPB;
    } else {
        tty.c_cflag &= ~CSTOPB;
    }

    // Enable receiver, ignore modem control lines.
    tty.c_cflag |= (CREAD | CLOCAL);

    // Non-blocking read semantics; the worker relies on poll() for readiness.
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 0;

    return ::tcsetattr(fd_, TCSANOW, &tty) == 0;
}

bool SourceLinuxImpl::toBaudConstant(uint32_t baudRate, unsigned& out) {
    switch (baudRate) {
        case 1200:    out = B1200;   return true;
        case 2400:    out = B2400;   return true;
        case 4800:    out = B4800;   return true;
        case 9600:    out = B9600;   return true;
        case 19200:   out = B19200;  return true;
        case 38400:   out = B38400;  return true;
        case 57600:   out = B57600;  return true;
        case 115200:  out = B115200; return true;
        case 230400:  out = B230400; return true;
        default:      return false;
    }
}

} // namespace dl24

#endif // defined(__linux__)
