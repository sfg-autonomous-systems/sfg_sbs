/// @file main.cpp
/// @brief Lifecycle suite: the battery's per-transaction state across handle_receive and handle_request, including
///        arming, one-shot replies, draining the bus and recovering after rejected frames.
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <unity.h>

#include "test_fixture.hpp"

/// @brief Answers ManufacturerName() with "Hi" and every other command with 0x1234, so reply bytes are known in advance.
/// @param command The command for which to provide a reply.
/// @return The value to return for the given command.
static sfg_sbs::Value known_reply(sfg_sbs::Command command)
{
    if (command == sfg_sbs::Command::ManufacturerName)
    {
        return sfg_sbs::Value{sfg_sbs::Block("Hi")};
    }
    return sfg_sbs::Value{sfg_sbs::Word(static_cast<std::uint16_t>(0x1234))};
}

/// @brief A battery drains every frame it refuses, so a leftover payload cannot poison the next frame.
void test_battery_drains_refused_frames()
{
    struct RefusedFrame
    {
    public:
        const char *m_message;
        std::uint8_t m_bytes[4];
        size_t m_size;
    };

    const RefusedFrame frames[] = {
        {"An unsupported command with a payload should be drained.", {0x30, 0x01, 0x02, 0x03}, 4},
        {"A truncated block write should be drained.", {static_cast<std::uint8_t>(sfg_sbs::Command::OptionalMfgFunction5), 0x05, 'a', 'b'}, 4},
        {"A write to a read-only command should be drained.", {static_cast<std::uint8_t>(sfg_sbs::Command::Voltage), 0x34, 0x12}, 3},
    };

    for (const auto &frame : frames)
    {
        TestFixture fixture;
        TestFixture::Battery battery(fixture.m_wire, known_reply, nullptr);
        fixture.m_rx_buffer.push(frame.m_bytes, frame.m_bytes + frame.m_size);
        battery.handle_receive(frame.m_size);

        TEST_ASSERT_EQUAL_UINT_MESSAGE(0, fixture.m_rx_buffer.size(), frame.m_message);
    }
}

/// @brief After refusing an unsupported write, the battery answers the next read request with that command's reply.
void test_battery_answers_the_next_frame_after_an_unsupported_write()
{
    TestFixture fixture;
    TestFixture::Battery battery(fixture.m_wire, known_reply, nullptr);

    // Frame 1: Reserved command with a 3-byte payload. No traits, so it is refused.
    const std::uint8_t unknown_command[] = {0x30, 0x01, 0x02, 0x03};
    fixture.m_rx_buffer.push(unknown_command, unknown_command + std::size(unknown_command));
    battery.handle_receive(std::size(unknown_command));

    // Frame 2: A fresh command-only read request for a supported block command.
    const std::uint8_t data[] = {static_cast<std::uint8_t>(sfg_sbs::Command::ManufacturerName)};
    fixture.m_rx_buffer.push(data, data + std::size(data));
    battery.handle_receive(std::size(data));
    battery.handle_request();

    TEST_ASSERT_EQUAL_UINT8(0x02, fixture.pop_tx());
    TEST_ASSERT_EQUAL_UINT8('H', fixture.pop_tx());
    TEST_ASSERT_EQUAL_UINT8('i', fixture.pop_tx());
}

/// @brief An armed command is answered once and a second request without a new receive reports NoActiveCommand.
void test_battery_reply_is_one_shot()
{
    TestFixture fixture;
    TestFixture::Battery battery(fixture.m_wire, known_reply, nullptr);
    const std::uint8_t data[] = {static_cast<std::uint8_t>(sfg_sbs::Command::Voltage)};
    fixture.m_rx_buffer.push(data, data + std::size(data));
    battery.handle_receive(std::size(data));
    battery.handle_request();
    const auto result = battery.handle_request();

    TEST_ASSERT_FALSE(result.has_value());

    if (!result.has_value())
    {
        TEST_ASSERT_TRUE(result.error() == sfg_sbs::Error::NoActiveCommand);
    }
}

/// @brief The battery refuses a receive whose command byte is not on the bus with NoDataAvailable.
/// @details The command byte is read from the bus and validated before it is cast into a command, so a buffer
///          underrun is reported as NoDataAvailable instead of arming a phantom command.
void test_battery_refuses_receive_on_empty_buffer()
{
    TestFixture fixture;
    TestFixture::Battery battery(fixture.m_wire, known_reply, nullptr);
    const auto received = battery.handle_receive(1);

    TEST_ASSERT_FALSE_MESSAGE(received.has_value(), "An empty receive buffer should be refused, not reported as success.");

    if (!received.has_value())
    {
        TEST_ASSERT_TRUE_MESSAGE(received.error() == sfg_sbs::Error::NoDataAvailable, "An underrun should report NoDataAvailable.");
    }
}

/// @brief A receive on an empty buffer arms no command, so the following request reports NoActiveCommand.
void test_battery_empty_receive_arms_no_command()
{
    TestFixture fixture;
    TestFixture::Battery battery(fixture.m_wire, known_reply, nullptr);
    battery.handle_receive(1);
    const auto requested = battery.handle_request();

    TEST_ASSERT_FALSE_MESSAGE(requested.has_value(), "No command should be armed, so the request phase fails.");

    if (!requested.has_value())
    {
        TEST_ASSERT_TRUE_MESSAGE(requested.error() == sfg_sbs::Error::NoActiveCommand, "The request should report NoActiveCommand, not a phantom UnknownCommand.");
    }
}

/// @brief Unity hook executed before each test case.
void setUp()
{
}

/// @brief Unity hook executed after each test case.
void tearDown()
{
}

/// @brief Test-program entry point for this suite.
/// @param argc Argument count, unused by the suite.
/// @param argv Argument vector, unused by the suite.
/// @return The number of failed test cases as reported by Unity. Zero means the whole suite passed.
int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_battery_drains_refused_frames);
    RUN_TEST(test_battery_answers_the_next_frame_after_an_unsupported_write);
    RUN_TEST(test_battery_reply_is_one_shot);
    RUN_TEST(test_battery_refuses_receive_on_empty_buffer);
    RUN_TEST(test_battery_empty_receive_arms_no_command);
    return UNITY_END();
}
