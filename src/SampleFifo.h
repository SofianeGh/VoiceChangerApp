// "SampleFifo.h"
#pragma once
#include <atomic>
#include <cstddef>
#include <vector>

// File circulaire sans verrou : un seul thread écrit, un seul thread lit (Real-Time Safe)
class SampleFifo {
public:
    explicit SampleFifo(size_t capacity) : m_buffer(capacity + 1, 0.0f) {}

    size_t Available() const {
        size_t write = m_write.load(std::memory_order_acquire);
        size_t read = m_read.load(std::memory_order_acquire);
        return write >= read ? write - read : write + m_buffer.size() - read;
    }

    // Renvoie le nombre d'échantillons réellement écrits (moins que count si la file est pleine)
    size_t Push(const float* samples, size_t count) {
        size_t write = m_write.load(std::memory_order_relaxed);
        size_t read = m_read.load(std::memory_order_acquire);
        size_t free = (read + m_buffer.size() - write - 1) % m_buffer.size();
        if (count > free) count = free;

        for (size_t i = 0; i < count; ++i) {
            m_buffer[write] = samples[i];
            write = (write + 1) % m_buffer.size();
        }
        m_write.store(write, std::memory_order_release);
        return count;
    }

    // Renvoie le nombre d'échantillons réellement lus
    size_t Pop(float* samples, size_t count) {
        size_t read = m_read.load(std::memory_order_relaxed);
        size_t write = m_write.load(std::memory_order_acquire);
        size_t available = write >= read ? write - read : write + m_buffer.size() - read;
        if (count > available) count = available;

        for (size_t i = 0; i < count; ++i) {
            samples[i] = m_buffer[read];
            read = (read + 1) % m_buffer.size();
        }
        m_read.store(read, std::memory_order_release);
        return count;
    }

private:
    std::vector<float> m_buffer;
    std::atomic<size_t> m_write{0};
    std::atomic<size_t> m_read{0};
};
