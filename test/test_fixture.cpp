#include "test_fixture.hpp"

TestFixture::TestFixture()
    : m_rx_buffer(),
      m_tx_buffer(),
      m_wire(m_rx_buffer, m_tx_buffer)
{
}

std::uint8_t TestFixture::pop_tx()
{
    const auto byte = m_tx_buffer.front();
    m_tx_buffer.pop();
    return byte;
}
