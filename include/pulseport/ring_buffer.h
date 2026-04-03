#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <optional>

namespace pulseport {

/// Lock-free single-producer single-consumer ring buffer.
/// Capacity must be a power of two.
template <typename T, size_t Capacity>
class RingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");

public:
    RingBuffer() = default;

    /// Push an item. Overwrites oldest if full (lossy for live telemetry).
    void push(const T& item) {
        size_t w = write_pos_.load(std::memory_order_relaxed);
        buffer_[w & kMask] = item;
        write_pos_.store(w + 1, std::memory_order_release);
    }

    /// Try to pop oldest unread item. Returns nullopt if empty.
    std::optional<T> pop() {
        size_t r = read_pos_.load(std::memory_order_relaxed);
        size_t w = write_pos_.load(std::memory_order_acquire);
        if (r == w) return std::nullopt;
        T item = buffer_[r & kMask];
        read_pos_.store(r + 1, std::memory_order_release);
        return item;
    }

    /// Number of unread items.
    size_t size() const {
        size_t w = write_pos_.load(std::memory_order_acquire);
        size_t r = read_pos_.load(std::memory_order_acquire);
        return w - r;
    }

    bool empty() const { return size() == 0; }

    /// Read last N items without consuming. Returns actual count read.
    /// Items are ordered oldest-first.
    size_t peek_last(T* out, size_t count) const {
        size_t w = write_pos_.load(std::memory_order_acquire);
        size_t available = w - read_pos_.load(std::memory_order_acquire);
        if (count > available) count = available;
        if (count > Capacity) count = Capacity;

        size_t start = w - count;
        for (size_t i = 0; i < count; ++i) {
            out[i] = buffer_[(start + i) & kMask];
        }
        return count;
    }

    static constexpr size_t capacity() { return Capacity; }

private:
    static constexpr size_t kMask = Capacity - 1;
    std::array<T, Capacity> buffer_{};
    alignas(64) std::atomic<size_t> write_pos_{0};
    alignas(64) std::atomic<size_t> read_pos_{0};
};

} // namespace pulseport
