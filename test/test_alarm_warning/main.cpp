/// @file main.cpp
/// @brief Alarm warning suite: which BatteryStatus() bits the battery broadcasts as AlarmWarning() (§5.4.1), to
///        which peer, and the word it sends.
#include <cstddef>
#include <cstdint>
#include <optional>
#include <unity.h>

#include "test_fixture.hpp"
#include "test_utils.hpp"

/// @brief SMBus addresses of the host and the charger.
static constexpr std::uint8_t s_host_address = 0x08;
static constexpr std::uint8_t s_charger_address = 0x09;

/// @brief A read handler answering zero; the broadcast path never calls it.
/// @return Zero as a word value.
static sfg_sbs::Value reply_zero(sfg_sbs::Command)
{
    return sfg_sbs::Value{sfg_sbs::Word(static_cast<std::uint16_t>(0))};
}

/// @brief Broadcasts @p battery_status from a battery that knows both peers and counts the frames sent to @p address.
/// @return The number of transmissions opened to @p address.
static size_t frames_sent_to(std::uint8_t address, std::uint16_t battery_status)
{
    TestFixture fixture;
    TestFixture::Battery battery(fixture.m_wire, reply_zero, nullptr, s_host_address, s_charger_address);
    battery.broadcast_alarm_warning_if_required(battery_status);

    return fixture.count_invocations<TestFixture::Wire::BeginTransmission>(
        [address](const TestFixture::Wire::BeginTransmission &invocation)
        {
            return invocation.m_address == address;
        });
}

/// @brief Broadcasts @p battery_status from a battery that knows both peers and counts every frame sent.
/// @return The number of transmissions opened.
static size_t frames_sent(std::uint16_t battery_status)
{
    TestFixture fixture;
    TestFixture::Battery battery(fixture.m_wire, reply_zero, nullptr, s_host_address, s_charger_address);
    battery.broadcast_alarm_warning_if_required(battery_status);
    return fixture.count_invocations<TestFixture::Wire::BeginTransmission>();
}

/// @brief Broadcasts @p battery_status to the host only and returns the word of the frame sent.
/// @return The broadcast word, reassembled low byte first.
static std::uint16_t broadcast_word(std::uint16_t battery_status)
{
    TestFixture fixture;
    TestFixture::Battery battery(fixture.m_wire, reply_zero, nullptr, s_host_address, std::nullopt);
    battery.broadcast_alarm_warning_if_required(battery_status);
    TEST_ASSERT_EQUAL_UINT(1 + sfg_sbs::Word::s_wire_size, fixture.m_tx_buffer.size());

    fixture.pop_tx();
    const std::uint8_t low = fixture.pop_tx();
    const std::uint8_t high = fixture.pop_tx();
    return static_cast<std::uint16_t>((high << 8) | low);
}

/// @brief Every alarm bit reaches exactly the peers §5.4.1 assigns it: the note on p.41 sends every alarm bit to
///        both peers, except REMAINING_CAPACITY_ALARM and REMAINING_TIME_ALARM, which go to the host only.
void test_alarm_bits_are_routed_per_spec()
{
    struct Route
    {
    public:
        const char *m_name;
        std::uint16_t m_bit;
        size_t m_frames_to_host;
        size_t m_frames_to_charger;
    };

    const Route routes[] = {
        {"OVER_CHARGED_ALARM", sfg_sbs::BatteryStatus::OverChargedAlarm, 1, 1},
        {"TERMINATE_CHARGE_ALARM", sfg_sbs::BatteryStatus::TerminateChargeAlarm, 1, 1},
        {"OVER_TEMP_ALARM", sfg_sbs::BatteryStatus::OverTempAlarm, 1, 1},
        {"TERMINATE_DISCHARGE_ALARM", sfg_sbs::BatteryStatus::TerminateDischargeAlarm, 1, 1},
        {"REMAINING_CAPACITY_ALARM", sfg_sbs::BatteryStatus::RemainingCapacityAlarm, 1, 0},
        {"REMAINING_TIME_ALARM", sfg_sbs::BatteryStatus::RemainingTimeAlarm, 1, 0},
    };

    for (const auto &route : routes)
    {
        TEST_ASSERT_EQUAL_UINT_MESSAGE(route.m_frames_to_host, frames_sent_to(s_host_address, route.m_bit), make_message(route.m_name, "should be routed to the host as specified.").c_str());
        TEST_ASSERT_EQUAL_UINT_MESSAGE(route.m_frames_to_charger, frames_sent_to(s_charger_address, route.m_bit), make_message(route.m_name, "should be routed to the charger as specified.").c_str());
    }
}

/// @brief An alarm bit for both peers sends two frames and no more.
void test_alarm_for_both_peers_sends_two_frames()
{
    TEST_ASSERT_EQUAL_UINT(2, frames_sent(sfg_sbs::BatteryStatus::TerminateDischargeAlarm));
}

/// @brief A battery without a charger address sends an alarm for both peers to the host only.
void test_alarm_skips_unconfigured_charger()
{
    TestFixture fixture;
    TestFixture::Battery battery(fixture.m_wire, reply_zero, nullptr, s_host_address, std::nullopt);
    battery.broadcast_alarm_warning_if_required(sfg_sbs::BatteryStatus::TerminateDischargeAlarm);

    TEST_ASSERT_EQUAL_UINT(1, fixture.count_invocations<TestFixture::Wire::BeginTransmission>());
}

/// @brief A battery without a host address sends an alarm for both peers to the charger only.
void test_alarm_skips_unconfigured_host()
{
    TestFixture fixture;
    TestFixture::Battery battery(fixture.m_wire, reply_zero, nullptr, std::nullopt, s_charger_address);
    battery.broadcast_alarm_warning_if_required(sfg_sbs::BatteryStatus::TerminateDischargeAlarm);

    TEST_ASSERT_EQUAL_UINT(1, fixture.count_invocations<TestFixture::Wire::BeginTransmission>());
}

/// @brief A broadcast frame is closed with a stop bit, so it releases the bus.
void test_broadcast_frame_ends_with_stop()
{
    TestFixture fixture;
    TestFixture::Battery battery(fixture.m_wire, reply_zero, nullptr, s_host_address, std::nullopt);
    battery.broadcast_alarm_warning_if_required(sfg_sbs::BatteryStatus::OverChargedAlarm);

    TEST_ASSERT_TRUE(fixture.get_invocation<TestFixture::Wire::EndTransmission>().m_stop_bit);
}

/// @brief The broadcast reports true when an alarm bit was sent.
void test_broadcast_reports_true_for_alarm_bit()
{
    TestFixture fixture;
    TestFixture::Battery battery(fixture.m_wire, reply_zero, nullptr, s_host_address, s_charger_address);
    const auto broadcast = battery.broadcast_alarm_warning_if_required(sfg_sbs::BatteryStatus::TerminateDischargeAlarm);

    TEST_ASSERT_TRUE(broadcast.has_value());
    TEST_ASSERT_TRUE(broadcast.value());
}

/// @brief The broadcast reports false when no alarm bit is set.
void test_broadcast_reports_false_without_alarm_bit()
{
    TestFixture fixture;
    TestFixture::Battery battery(fixture.m_wire, reply_zero, nullptr, s_host_address, s_charger_address);
    const auto broadcast = battery.broadcast_alarm_warning_if_required(sfg_sbs::BatteryStatus::s_status_bits_mask);

    TEST_ASSERT_TRUE(broadcast.has_value());
    TEST_ASSERT_FALSE(broadcast.value());
}

/// @brief Bits outside the alarm bits (error code, status and reserved bits) trigger no broadcast to either peer.
void test_non_alarm_bits_are_not_broadcast()
{
    TEST_ASSERT_EQUAL_UINT(0, frames_sent(static_cast<std::uint16_t>(~sfg_sbs::BatteryStatus::s_alarm_bits_mask)));
}

/// @brief The broadcast frame carries the AlarmWarning() command code 0x16 (§5.4.1).
void test_broadcast_frame_starts_with_alarm_warning_command()
{
    TestFixture fixture;
    TestFixture::Battery battery(fixture.m_wire, reply_zero, nullptr, s_host_address, std::nullopt);
    battery.broadcast_alarm_warning_if_required(sfg_sbs::BatteryStatus::OverChargedAlarm);

    TEST_ASSERT_EQUAL_UINT8(0x16, fixture.pop_tx());
}

/// @brief The broadcast word is the BatteryStatus() word with the error-code nibble set high (§5.4, §5.4.1).
void test_broadcast_word_is_battery_status_with_error_nibble_set()
{
    const auto battery_status = static_cast<std::uint16_t>(
        sfg_sbs::BatteryStatus::OverChargedAlarm |
        sfg_sbs::BatteryStatus::TerminateDischargeAlarm |
        sfg_sbs::BatteryStatus::Initialized |
        sfg_sbs::BatteryStatus::FullyCharged);

    TEST_ASSERT_EQUAL_HEX16(battery_status | sfg_sbs::BatteryStatus::s_error_code_mask, broadcast_word(battery_status));
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
    RUN_TEST(test_alarm_bits_are_routed_per_spec);
    RUN_TEST(test_alarm_for_both_peers_sends_two_frames);
    RUN_TEST(test_alarm_skips_unconfigured_charger);
    RUN_TEST(test_alarm_skips_unconfigured_host);
    RUN_TEST(test_broadcast_frame_ends_with_stop);
    RUN_TEST(test_broadcast_reports_true_for_alarm_bit);
    RUN_TEST(test_broadcast_reports_false_without_alarm_bit);
    RUN_TEST(test_non_alarm_bits_are_not_broadcast);
    RUN_TEST(test_broadcast_frame_starts_with_alarm_warning_command);
    RUN_TEST(test_broadcast_word_is_battery_status_with_error_nibble_set);
    return UNITY_END();
}
