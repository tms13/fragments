#ifndef TRIPLE_BUFFER_HPP
#define TRIPLE_BUFFER_HPP

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>

template<typename T>
class triple_buffer
{
    // the actual buffer
    T buffer[3] = {};

    // the roles the buffers currently have
    // read and write buffers are private to each side
    // the available buffer is passed between them
    T* readbuffer = &buffer[0];
    T* writebuffer = &buffer[1];
    std::atomic<T*> available = &buffer[2];

    // the last fully-written buffer waiting ready for reader
    std::atomic<T*> next_read_buf = nullptr;

    // When the reader catches up, it needs to wait for writer (slow path only)
    std::mutex read_queue_mutex = {};
    std::condition_variable read_queue = {};

public:

    // Writer interface

    // Writer has ownership of this buffer (this function never blocks).
    T *get_write_buffer()
    {
        return writebuffer;
    }

    // Writer releases ownership of its buffer.
    void set_write_complete()
    {
        // give back the write buffer
        auto *written = writebuffer;
        writebuffer = available.exchange(writebuffer);
        // mark it as written
        next_read_buf.store(written, std::memory_order_release);
        // notify any waiting reader
        read_queue.notify_one();
    }

    // Reader interface

    // Reader gets ownership of the buffer, until the next call of
    // get_read_buffer().
    T *get_read_buffer(std::chrono::milliseconds timeout = std::chrono::milliseconds::max())
    {
        auto const timeout_time = std::chrono::steady_clock::now() + timeout;

        // get the written buffer, waiting if necessary
        auto *b = next_read_buf.exchange(nullptr);
        while (b != readbuffer) {
            // it could be the available buffer
            readbuffer = available.exchange(readbuffer);
            if (b == readbuffer) {
                // yes, that's it
                return readbuffer;
            }
            // else we need to wait for writer
            b = nullptr;
            std::unique_lock lock{read_queue_mutex};
            auto test = [this,&b]{ b = next_read_buf.exchange(nullptr); return b; };
            if (!read_queue.wait_until(lock, timeout_time, test)) {
                return nullptr;
            }
        }

        return readbuffer;
    }


    // The unit test helper is enabled only if <gtest.h> is included before this header.
    // It's not available (or necessary) in production code.
#ifdef TEST
    // N.B. not thread-safe - only call this when reader and writer are idle
    void test_invariant(const char *file, int line) const
    {
        const std::set<const T*> buffers{&buffer[0], &buffer[1], &buffer[2]};
        const std::set<const T*> roles{readbuffer, available, writebuffer};
        auto const fail = buffers != roles
            || next_read_buf && !buffers.count(next_read_buf)
            || next_read_buf == writebuffer;
        if (fail) {
            auto name = [this](const T *slot){
                if (slot == &buffer[0]) { return "buffer[0]"; }
                if (slot == &buffer[1]) { return "buffer[1]"; }
                if (slot == &buffer[2]) { return "buffer[2]"; }
                if (slot == nullptr) { return "(null)"; }
                return "(invalid)";
            };
            ADD_FAILURE_AT(file, line) <<
                "Buffer/role mismatch:\n"
                "Buffers = " << &buffer[0] << ", " << &buffer[1] << ", " << &buffer[2] << "\n"
                "Read = " << readbuffer << " = " << name(readbuffer) << "\n"
                "Available = " << available  << " = "<< name(available) << "\n"
                "Write = " << writebuffer << " = " << name(writebuffer) << "\n"
                "Next Read = " << next_read_buf << " = " << name(next_read_buf) << "\n";
        }
    }
#endif
};

#endif // TRIPLE_BUFFER_HPP
