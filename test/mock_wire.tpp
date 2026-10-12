#include "mock_wire.hpp"

#include <algorithm>

template <size_t Capacity>
MockWire<Capacity>::MockWire(
    etl::circular_buffer<std::uint8_t, Capacity> &rx_buffer,
    etl::circular_buffer<std::uint8_t, Capacity> &tx_buffer)
    : m_rx_buffer(rx_buffer),
      m_tx_buffer(tx_buffer)

{
}

template <size_t Capacity>
void MockWire<Capacity>::beginTransmission(std::uint8_t address)
{
    m_invocations.push_back(BeginTransmission{address});
}

template <size_t Capacity>
std::uint8_t MockWire<Capacity>::endTransmission(bool stop_bit)
{
    std::uint8_t return_value = 0;
    m_invocations.push_back(EndTransmission{stop_bit, return_value});
    return return_value;
}

template <size_t Capacity>
int MockWire<Capacity>::read()
{
    auto return_value = 0;

    if (m_rx_buffer.empty())
    {
        return_value = -1;
    }
    else
    {
        return_value = static_cast<int>(m_rx_buffer.front());
        m_rx_buffer.pop();
    }
    m_invocations.push_back(Read{return_value});
    return return_value;
}

template <size_t Capacity>
void MockWire<Capacity>::write(std::uint8_t data)
{
    m_tx_buffer.push(data);
    m_invocations.push_back(Write{etl::vector<std::uint8_t, Capacity>(&data, &data + 1)});
}

template <size_t Capacity>
void MockWire<Capacity>::write(const std::uint8_t *data, size_t length)
{
    m_tx_buffer.push(data, data + length);
    m_invocations.push_back(Write{etl::vector<std::uint8_t, Capacity>(data, data + length)});
}

template <size_t Capacity>
size_t MockWire<Capacity>::requestFrom(std::uint8_t address, size_t length, bool stop_bit)
{
    // A read request clocks at most "length" bytes from the peer, so whatever the peer queued beyond
    // that point never arrives and is dropped here. The returned count is the number of bytes the
    // caller can subsequently read.
    const size_t received = std::min(m_rx_buffer.size(), length);

    while (m_rx_buffer.size() > received)
    {
        m_rx_buffer.pop();
    }
    m_invocations.push_back(RequestFrom{address, length, stop_bit, received});
    return received;
}

template <size_t Capacity>
int MockWire<Capacity>::available()
{
    auto available = static_cast<int>(m_rx_buffer.size());
    m_invocations.push_back(Available{available});
    return available;
}

template <size_t Capacity>
const std::vector<typename MockWire<Capacity>::Invocation> &MockWire<Capacity>::get_invocations() const
{
    return m_invocations;
}