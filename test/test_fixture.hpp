#pragma once

/// @file test_fixture.hpp
/// @brief Bus buffers and mock transport shared by every test group.
#include <cstddef>
#include <cstdint>
#include <string>
#include <unity.h>

#include "mock_wire.hpp"
#include "sfg_sbs/sbs.hpp"

/// @brief Per-test-case bus buffers and the mock transport built on top of them.
struct TestFixture
{
public:
    /// @brief Size of the emulated bus buffers, in bytes.
    static constexpr size_t s_buffer_capacity = 64;

    using Wire = MockWire<s_buffer_capacity>;
    using Battery = sfg_sbs::Battery<Wire>;
    using Host = sfg_sbs::Host<Wire>;

    TestFixture();

    /// @brief Removes and returns the oldest byte the code under test wrote to the bus.
    /// @return The oldest transmitted byte.
    std::uint8_t pop_tx();

    /// @brief Counts the recorded bus calls of type @p TInvocation.
    /// @return The number of matching calls.
    template <typename TInvocation>
    size_t count_invocations() const;

    /// @brief Counts the recorded bus calls of type @p TInvocation that satisfy @p predicate.
    /// @return The number of matching calls.
    template <typename TInvocation, typename TPredicate>
    size_t count_invocations(TPredicate predicate) const;

    /// @brief Returns the position of a recorded bus call in the call log, failing the test if it was never made.
    /// @param occurrence Zero-based index among the calls of type @p TInvocation.
    /// @return The call's position among all recorded calls.
    template <typename TInvocation>
    size_t get_invocation_index(size_t occurrence = 0) const;

    /// @brief Returns a recorded bus call, failing the test if it was never made.
    /// @param occurrence Zero-based index among the calls of type @p TInvocation.
    /// @return The recorded call.
    template <typename TInvocation>
    const TInvocation &get_invocation(size_t occurrence = 0) const;

    etl::circular_buffer<std::uint8_t, s_buffer_capacity> m_rx_buffer;
    etl::circular_buffer<std::uint8_t, s_buffer_capacity> m_tx_buffer;
    Wire m_wire;
};

#include "test_fixture.tpp"
