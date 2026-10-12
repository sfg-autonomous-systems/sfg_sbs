#pragma once

#include <cstdint>
#include <etl/circular_buffer.h>
#include <etl/variant.h>
#include <etl/vector.h>
#include <vector>

template <size_t Capacity>
class MockWire
{
public:
    struct BeginTransmission
    {
    public:
        std::uint8_t m_address;
    };

    struct EndTransmission
    {
    public:
        bool m_stop_bit;
        std::uint8_t m_return_value;
    };

    struct Read
    {
    public:
        int m_return_value;
    };

    struct Write
    {
    public:
        etl::vector<std::uint8_t, Capacity> m_data;
    };

    struct RequestFrom
    {
    public:
        std::uint8_t m_address;
        size_t m_length;
        bool m_stop_bit;
        size_t m_return_value;
    };

    struct Available
    {
    public:
        int m_return_value;
    };

    using Invocation = etl::variant<
        BeginTransmission,
        EndTransmission,
        Read,
        Write,
        RequestFrom,
        Available>;

    MockWire(
        etl::circular_buffer<std::uint8_t, Capacity> &rx_buffer,
        etl::circular_buffer<std::uint8_t, Capacity> &tx_buffer);
    void beginTransmission(std::uint8_t address);
    std::uint8_t endTransmission(bool stop_bit = true);
    int read();
    void write(std::uint8_t data);
    void write(const std::uint8_t *data, size_t length);
    size_t requestFrom(std::uint8_t address, size_t length, bool stop_bit = true);
    int available();

    const std::vector<Invocation> &get_invocations() const;

private:
    etl::circular_buffer<std::uint8_t, Capacity> &m_rx_buffer;
    etl::circular_buffer<std::uint8_t, Capacity> &m_tx_buffer;

    // It is fine to use std::vector here because tests do not run on embedded.
    std::vector<Invocation> m_invocations;
};

#include "mock_wire.tpp"