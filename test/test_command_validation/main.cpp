/// @file main.cpp
/// @brief Command validation suite: access rights, value/protocol agreement and unknown commands, on both the host
///        and the battery side.
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <unity.h>

#include "test_fixture.hpp"
#include "test_utils.hpp"

/// @brief Records whether the battery's write handler was invoked.
static bool s_write_called = false;

/// @brief A read handler answering zero, so reply bytes are known in advance.
/// @return Zero as a word value.
static sfg_sbs::Value reply_zero(sfg_sbs::Command)
{
    return sfg_sbs::Value{sfg_sbs::Word(static_cast<std::uint16_t>(0))};
}

/// @brief A write handler recording that it was invoked.
static void capture_write(sfg_sbs::Command, const sfg_sbs::Value &)
{
    s_write_called = true;
}

/// @brief Queues a Write Word to a read-only command for the battery.
/// @return The number of bytes queued.
static size_t push_write_to_read_only_command(TestFixture &fixture)
{
    const std::uint8_t frame[] = {static_cast<std::uint8_t>(sfg_sbs::Command::Voltage), 0x34, 0x12};
    fixture.m_rx_buffer.push(frame, frame + std::size(frame));
    return std::size(frame);
}

/// @brief A host write the host must refuse before it touches the bus.
struct RefusedWrite
{
public:
    const char *m_subject;
    sfg_sbs::Command m_command;
    sfg_sbs::Value m_value;
};

/// @brief A word written to read-only Voltage().
static const RefusedWrite s_write_to_read_only_command{
    "A write to a read-only command",
    sfg_sbs::Command::Voltage,
    sfg_sbs::Value{sfg_sbs::Word(static_cast<std::uint16_t>(0x1234))}};

/// @brief A word written to the writable block command OptionalMfgFunction5().
static const RefusedWrite s_word_for_block_command{
    "A word written to a block command",
    sfg_sbs::Command::OptionalMfgFunction5,
    sfg_sbs::Value{sfg_sbs::Word(static_cast<std::uint16_t>(7))}};

/// @brief A block written to the writable word command BatteryMode().
static const RefusedWrite s_block_for_word_command{
    "A block written to a word command",
    sfg_sbs::Command::BatteryMode, sfg_sbs::Value{sfg_sbs::Block{}}};

/// @brief The host refuses a write to a read-only command with AccessDenied.
void test_host_rejects_write_to_read_only_command()
{
    TestFixture fixture;
    TestFixture::Host host(fixture.m_wire);
    const auto result = host.write<sfg_sbs::Command::Voltage>(static_cast<std::uint16_t>(0x1234));

    TEST_ASSERT_FALSE_MESSAGE(result.has_value(), "A write to a read-only command should be refused.");

    if (!result.has_value())
    {
        TEST_ASSERT_TRUE_MESSAGE(result.error() == sfg_sbs::Error::AccessDenied, "The refusal should report AccessDenied.");
    }
}

/// @brief The battery refuses a write to a read-only command with AccessDenied.
/// @details The rejection is decided on the command byte, before the payload is parsed.
void test_battery_rejects_write_to_read_only_command()
{
    TestFixture fixture;
    TestFixture::Battery battery(fixture.m_wire, reply_zero, capture_write);
    const size_t queued = push_write_to_read_only_command(fixture);
    const auto result = battery.handle_receive(queued);

    TEST_ASSERT_FALSE_MESSAGE(result.has_value(), "A write to a read-only command should be refused.");

    if (!result.has_value())
    {
        TEST_ASSERT_TRUE_MESSAGE(result.error() == sfg_sbs::Error::AccessDenied, "The refusal should report AccessDenied.");
    }
}

/// @brief A write to a read-only command never reaches the battery's write handler.
void test_battery_does_not_route_write_to_read_only_command()
{
    TestFixture fixture;
    TestFixture::Battery battery(fixture.m_wire, reply_zero, capture_write);
    const size_t queued = push_write_to_read_only_command(fixture);
    battery.handle_receive(queued);

    TEST_ASSERT_FALSE(s_write_called);
}

/// @brief The host rejects a runtime write whose value type disagrees with the command's protocol with
///        UnexpectedValueType.
/// @details Appendix B (printed p. 48) defines word/int/unsigned int/Boolean as the same 16-bit word, so the
///          runtime write path validates the transfer protocol only; sign is a property of the command and is
///          enforced by the typed API at compile time. The targets are writable, so the access check cannot mask the result.
void test_host_rejects_mismatched_value_type()
{
    const RefusedWrite writes[] = {s_word_for_block_command, s_block_for_word_command};

    for (const auto &write : writes)
    {
        TestFixture fixture;
        TestFixture::Host host(fixture.m_wire);
        const auto result = host.write(write.m_command, write.m_value);

        TEST_ASSERT_FALSE_MESSAGE(result.has_value(), make_message(write.m_subject, "should be refused.").c_str());

        if (!result.has_value())
        {
            TEST_ASSERT_TRUE_MESSAGE(result.error() == sfg_sbs::Error::UnexpectedValueType, make_message(write.m_subject, "should report UnexpectedValueType.").c_str());
        }
    }
}

/// @brief The host validates a write before beginTransmission, so a refused write never touches the bus.
void test_host_refused_write_emits_nothing()
{
    const RefusedWrite writes[] = {s_write_to_read_only_command, s_word_for_block_command, s_block_for_word_command};

    for (const auto &write : writes)
    {
        TestFixture fixture;
        TestFixture::Host host(fixture.m_wire);
        host.write(write.m_command, write.m_value);

        TEST_ASSERT_EQUAL_UINT_MESSAGE(0, fixture.m_wire.get_invocations().size(), make_message(write.m_subject, "should not touch the bus.").c_str());
    }
}

/// @brief The battery refuses a frame for a command code without traits with UnknownCommand.
void test_battery_rejects_unknown_command()
{
    TestFixture fixture;
    TestFixture::Battery battery(fixture.m_wire, reply_zero, capture_write);
    const std::uint8_t frame[] = {0x30};
    fixture.m_rx_buffer.push(frame, frame + std::size(frame));
    const auto result = battery.handle_receive(std::size(frame));

    TEST_ASSERT_FALSE(result.has_value());

    if (!result.has_value())
    {
        TEST_ASSERT_TRUE(result.error() == sfg_sbs::Error::UnknownCommand);
    }
}

/// @brief Unity hook executed before each test case.
void setUp()
{
    s_write_called = false;
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
    RUN_TEST(test_host_rejects_write_to_read_only_command);
    RUN_TEST(test_host_refused_write_emits_nothing);
    RUN_TEST(test_battery_rejects_write_to_read_only_command);
    RUN_TEST(test_battery_does_not_route_write_to_read_only_command);
    RUN_TEST(test_host_rejects_mismatched_value_type);
    RUN_TEST(test_battery_rejects_unknown_command);
    return UNITY_END();
}
