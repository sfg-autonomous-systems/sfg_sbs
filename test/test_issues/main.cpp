/// @file main.cpp
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <type_traits>
#include <unity.h>
#include <utility>

#include "test_fixture.hpp"

/// @brief Records the value the battery's write handler received, so write routing is assertable.
static sfg_sbs::Command s_write_command = sfg_sbs::Command::Voltage;
static bool s_write_called = false;
static sfg_sbs::Value s_written_value{};

/// @brief A read handler answering zero, so reply bytes are known in advance.
/// @return Zero as a word value.
static sfg_sbs::Value reply_zero(sfg_sbs::Command)
{
    return sfg_sbs::Value{sfg_sbs::Word(static_cast<std::uint16_t>(0))};
}

/// @brief A read handler whose low nibble is occupied, to expose the BatteryStatus() error nibble.
/// @return 0xBEEF as a word value.
static sfg_sbs::Value reply_busy_nibble(sfg_sbs::Command)
{
    return sfg_sbs::Value{sfg_sbs::Word(static_cast<std::uint16_t>(0xBEEF))};
}

/// @brief A read handler answering a word whose LSB is the command code, so the answered register is identifiable.
/// @return 0x11nn as a word value, nn being the command code.
static sfg_sbs::Value reply_command_code(sfg_sbs::Command command)
{
    return sfg_sbs::Value{sfg_sbs::Word(static_cast<std::uint16_t>(0x1100 | static_cast<std::uint8_t>(command)))};
}

/// @brief A write handler capturing the routed command and value.
static void capture_write(sfg_sbs::Command command, const sfg_sbs::Value &value)
{
    s_write_called = true;
    s_write_command = command;
    s_written_value = value;
}

/// @brief Detects whether Battery offers a ChargingCurrent()/ChargingVoltage() broadcast member.
/// @tparam TBattery The battery specialisation to probe.
/// @details True when the member is absent, i.e. the §5.2 broadcast API does not exist.
template <typename TBattery, typename = void>
struct battery_lacks_charging_broadcast : std::true_type
{
};

template <typename TBattery>
struct battery_lacks_charging_broadcast<
    TBattery,
    std::void_t<decltype(std::declval<TBattery &>().broadcast_charging_values_if_required())>> : std::false_type
{
};

/// @brief Detects whether Host offers a receive entry point for a broadcast frame.
/// @tparam THost The host specialisation to probe.
/// @details True when the member is absent, i.e. the peer a §5.4.1 push is addressed to cannot take it.
template <typename THost, typename = void>
struct host_lacks_broadcast_receive_api : std::true_type
{
};

template <typename THost>
struct host_lacks_broadcast_receive_api<
    THost,
    std::void_t<decltype(std::declval<THost &>().handle_broadcast_receive(std::declval<std::size_t>()))>> : std::false_type
{
};

/// Issue #1: OBSERVED battery transaction errors never reach the BatteryStatus() nibble.
void test_issue_1_transaction_error_leaves_battery_status_untouched()
{
    TestFixture fixture;
    TestFixture::Battery battery(fixture.m_wire, reply_busy_nibble, nullptr);

    // Transaction 1: write to a reserved command code; Appendix C demands error code 0x0002.
    const std::uint8_t reserved_write[] = {0x30, 0x01, 0x02, 0x03};
    fixture.m_rx_buffer.push(reserved_write, reserved_write + std::size(reserved_write));
    const auto received = battery.handle_receive(std::size(reserved_write));
    TEST_ASSERT_TRUE(!received.has_value());
    TEST_ASSERT_TRUE(received.error() == sfg_sbs::Error::UnknownCommand);

    // Transaction 2: read BatteryStatus(); §4.3.1 requires the recorded 0x0002.
    const std::uint8_t status_read[] = {0x16};
    fixture.m_rx_buffer.push(status_read, status_read + std::size(status_read));
    battery.handle_receive(std::size(status_read));
    battery.handle_request();

    TEST_ASSERT_EQUAL_UINT(2, fixture.m_tx_buffer.size());
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0x0F, fixture.m_tx_buffer.front() & 0x0F, "observed: handler nibble passes through; §4.3.1 requires the transaction's error code");
}

/// Issue #5: OBSERVED a read of a command without traits is answered with zero filler; §4.3.2 requires
/// a NACK or an SCL timeout. Deliberate: Wire cannot NACK a chosen byte, and hosts such as the
/// Turtlebot 4 treat a held clock as a dead battery. The filler stays after the fix, so this pin keeps
/// passing and must be rewritten together with the fix.
void test_issue_5_request_of_unknown_command_emits_filler()
{
    TestFixture fixture;
    TestFixture::Battery battery(fixture.m_wire, reply_busy_nibble, nullptr);
    const std::uint8_t read_request[] = {0x37};
    fixture.m_rx_buffer.push(read_request, read_request + std::size(read_request));
    const auto received = battery.handle_receive(std::size(read_request));

    TEST_ASSERT_TRUE(!received.has_value());
    TEST_ASSERT_TRUE(received.error() == sfg_sbs::Error::UnknownCommand);

    battery.handle_request();

    TEST_ASSERT_EQUAL_UINT_MESSAGE(sfg_sbs::Value::s_max_wire_size, fixture.m_tx_buffer.size(), "observed: zero filler instead of an abort; §4.3.2 expects a NACK or timeout");

    while (!fixture.m_tx_buffer.empty())
    {
        TEST_ASSERT_EQUAL_UINT8(0x00, fixture.m_tx_buffer.front());
        fixture.m_tx_buffer.pop();
    }
}

/// Issue #1: OBSERVED a block write with an incorrect data-block size is refused with BadSize,
/// and no Appendix C code is recorded: a Battery instance keeps no transaction error register, so the mandated
/// BadSize (0x0006) has no observable home. The capacity refusal itself is asserted as specified
/// behaviour by test_battery_rejects_block_write_over_max_data_size.
/// The frame targets the writable block-capable row (0x2F) so the count byte reaches the capacity check.
void test_issue_1_oversized_block_count_reports_no_bad_size()
{
    TestFixture fixture;
    TestFixture::Battery battery(fixture.m_wire, reply_zero, capture_write);
    std::uint8_t frame[34];
    frame[0] = static_cast<std::uint8_t>(sfg_sbs::Command::OptionalMfgFunction5);
    frame[1] = sfg_sbs::Block::s_max_data_size + 1;

    for (size_t index = 0; index < sfg_sbs::Block::s_max_data_size + 1; index++)
    {
        frame[index + 2] = static_cast<std::uint8_t>('a');
    }
    fixture.m_rx_buffer.push(frame, frame + std::size(frame));
    const auto received = battery.handle_receive(std::size(frame));

    TEST_ASSERT_TRUE_MESSAGE(!received.has_value(), "observed: an oversized block count is accepted");

    if (!received.has_value())
    {
        TEST_ASSERT_TRUE_MESSAGE(received.error() == sfg_sbs::Error::BadSize, "observed: unexpected code for an oversized block count");
    }
    TEST_ASSERT_FALSE(s_write_called);

    const std::uint8_t status_read[] = {0x16};
    fixture.m_rx_buffer.push(status_read, status_read + std::size(status_read));
    battery.handle_receive(std::size(status_read));
    battery.handle_request();

    TEST_ASSERT_EQUAL_UINT(2, fixture.m_tx_buffer.size());
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(
        0x00,
        fixture.m_tx_buffer.front() & 0x0F,
        "observed: the reply nibble is the handler's, not a recorded code; Appendix C requires BadSize (0x0006)");
}

/// Issue #2: OBSERVED broadcasts ignore ALARM_MODE and CHARGER_MODE.
void test_issue_2_broadcast_ignores_battery_mode_bits()
{
    TestFixture fixture;
    TestFixture::Battery battery(fixture.m_wire, reply_zero, nullptr, 0x08, 0x09);
    // A host would set ALARM_MODE (bit 13) and CHARGER_MODE (bit 14) in BatteryMode() (0x6000), but the battery
    // has no mode state to hand them to, so the broadcast below cannot observe them.
    const auto broadcast = battery.broadcast_alarm_warning_if_required(0x8000);

    TEST_ASSERT_TRUE(broadcast.has_value());
    TEST_ASSERT_TRUE(broadcast.value());
    TEST_ASSERT_EQUAL_UINT_MESSAGE(6, fixture.m_tx_buffer.size(), "observed: both peers are addressed with the bits set");
}

/// Issue #3: OBSERVED broadcasts carry no cadence; an alarm repeats immediately at every call.
void test_issue_3_broadcast_repeats_without_cadence()
{
    TestFixture fixture;
    TestFixture::Battery battery(fixture.m_wire, reply_zero, nullptr, 0x08, 0x09);
    const auto first = battery.broadcast_alarm_warning_if_required(0x8000);
    const auto second = battery.broadcast_alarm_warning_if_required(0x8000);

    TEST_ASSERT_TRUE(first.has_value() && second.has_value());
    TEST_ASSERT_EQUAL_UINT8(1, static_cast<std::uint8_t>(first.value()));
    TEST_ASSERT_EQUAL_UINT8(1, static_cast<std::uint8_t>(second.value()));
    TEST_ASSERT_EQUAL_UINT_MESSAGE(12, fixture.m_tx_buffer.size(), "observed: immediate repeat; §5.4.1 prescribes a 10 second cadence");
}

/// Issue #3: OBSERVED no "On State" suppression window (§4.4.2, §5.4).
void test_issue_3_no_on_state_suppression_window()
{
    TestFixture fixture;
    TestFixture::Battery battery(fixture.m_wire, reply_zero, nullptr, 0x08, 0x09);
    // First broadcast after construction, i.e. immediately after the battery "enters the On State".
    const auto broadcast = battery.broadcast_alarm_warning_if_required(0x0200);

    TEST_ASSERT_TRUE(broadcast.has_value());
    TEST_ASSERT_TRUE_MESSAGE(broadcast.value(), "observed: broadcast on the first call; §4.4.2 forbids broadcasting within 10s of the On State");
}

/// Issue #4: OBSERVED the battery API offers no ChargingCurrent()/ChargingVoltage() broadcast (§5.2).
void test_issue_4_no_charging_broadcast_api()
{
    TEST_ASSERT_TRUE_MESSAGE(battery_lacks_charging_broadcast<TestFixture::Battery>::value, "observed: Battery has no charging-broadcast member; §5.2 requires it");
}

/// Issue #13: OBSERVED the AlarmWarning() command byte is the literal 0x16, the default battery address 0x0B shifted left.
/// @details §5.4.1/§6.3 require the battery's own address in the command-code slot; the constructor never receives
///          that address, so a battery at any other address still sends 0x16. 0x16 stays correct at the default
///          address, so this pin keeps passing after the fix and must be rewritten with a non-default address.
void test_issue_13_broadcast_command_byte_is_hardcoded()
{
    TestFixture fixture;
    TestFixture::Battery battery(fixture.m_wire, reply_zero, nullptr, 0x08, std::nullopt);
    battery.broadcast_alarm_warning_if_required(0x1000);

    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x16, fixture.pop_tx(), "observed: literal 0x16, valid only while the battery sits at 0x0B; §6.3 requires the battery's own address");
}

/// Issue #6: OBSERVED BatteryMode() writes are unvalidated, including reserved and read-only bits.
void test_issue_6_battery_mode_write_is_unvalidated()
{
    TestFixture fixture;
    TestFixture::Battery battery(fixture.m_wire, reply_zero, capture_write);
    const std::uint8_t frame[] = {0x03, 0xFF, 0xFF};
    fixture.m_rx_buffer.push(frame, frame + std::size(frame));
    const auto result = battery.handle_receive(std::size(frame));

    TEST_ASSERT_TRUE_MESSAGE(result.has_value(), "observed: an all-ones BatteryMode() write is accepted; §5.1.4 prohibits setting reserved MSB bits");
    TEST_ASSERT_TRUE(s_write_called);
    TEST_ASSERT_EQUAL_UINT16(0xFFFF, etl::get<sfg_sbs::Word>(s_written_value).as_uint());

    TestFixture host_fixture;
    TestFixture::Host host(host_fixture.m_wire);
    const auto host_write = host.write<sfg_sbs::Command::BatteryMode>(static_cast<std::uint16_t>(0xFFFF));

    TEST_ASSERT_TRUE_MESSAGE(host_write.has_value(), "observed: the host can write reserved BatteryMode() bits");
}

/// Issue #7: OBSERVED the host discards the battery's error nibble: a word with a populated nibble is returned verbatim.
void test_issue_7_host_read_returns_error_nibble_verbatim()
{
    TestFixture fixture;
    TestFixture::Host host(fixture.m_wire);
    const std::uint8_t status_frame[] = {0x02, 0x60}; // BatteryStatus() carrying error code 0x0002.
    fixture.m_rx_buffer.push(status_frame, status_frame + std::size(status_frame));
    const auto value = host.read<sfg_sbs::Command::BatteryStatus>();

    TEST_ASSERT_TRUE(value.has_value());
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(0x6002, value.value(), "observed: raw word, the API exposes no error-code handling");
}

/// Issue #9: OBSERVED the host API offers no receive entry point, so a pushed broadcast has no taker (§5.4.1).
/// @details Probes Host only. A receiver built as a separate component leaves this pin passing, so it must be
///          rewritten together with the fix.
void test_issue_9_host_has_no_broadcast_receive_api()
{
    TEST_ASSERT_TRUE_MESSAGE((host_lacks_broadcast_receive_api<TestFixture::Host>::value), "observed: Host has no receive entry point; §5.4.1 pushes are addressed to a battery the host must service");
}

/// Issue #9: OBSERVED the battery path refuses the frame the broadcast sends, so it is not a host-side receiver.
/// @details The push arrives as a 3-byte write to 0x16, which the battery-function traits mark read-only, so
///          handle_receive() rejects it before routing; a host built on this path never sees the alarm.
void test_issue_9_battery_path_refuses_a_broadcast_frame()
{
    TestFixture fixture;
    TestFixture::Battery battery(fixture.m_wire, reply_zero, capture_write);
    // The frame consists of an AlarmWarning() word 0x80AF: command byte, LSB, MSB.
    const std::uint8_t frame[] = {0x16, 0xAF, 0x80};
    fixture.m_rx_buffer.push(frame, frame + std::size(frame));
    const auto result = battery.handle_receive(std::size(frame));

    TEST_ASSERT_TRUE_MESSAGE(!result.has_value(), "observed: the frame is rejected; specified: a host routes it as an AlarmWarning() push (§5.4.1)");

    if (!result.has_value())
    {
        TEST_ASSERT_TRUE_MESSAGE(result.error() == sfg_sbs::Error::AccessDenied, "observed: AccessDenied, the push is scored as a write to the read-only BatteryStatus()");
    }
    TEST_ASSERT_FALSE_MESSAGE(s_write_called, "observed: the alarm never reaches the application handler");
}

/// Issue #10: OBSERVED SpecificationInfo() defaults to 0x0010, i.e. version 1.0 with the reserved revision 0.
void test_issue_10_specification_info_default_is_not_v1_1()
{
    const auto value = etl::get<sfg_sbs::Word>(sfg_sbs::get_default_value(sfg_sbs::Command::SpecificationInfo)).as_uint();

    TEST_ASSERT_EQUAL_HEX16_MESSAGE(0x0010, value, "observed: 0x0010 (version 1.0, reserved revision 0); §5.1.25 encodes v1.1 as 0x0021");
}

/// Issue #11: OBSERVED a read request the host never clocks out stays armed across a refused receive, so the next
/// request answers the stale register instead of filler.
void test_issue_11_orphaned_read_answers_next_unknown_request()
{
    TestFixture fixture;
    TestFixture::Battery battery(fixture.m_wire, reply_command_code, nullptr);
    const std::uint8_t orphaned_read[] = {0x09};
    fixture.m_rx_buffer.push(orphaned_read, orphaned_read + std::size(orphaned_read));
    battery.handle_receive(std::size(orphaned_read));
    const std::uint8_t unknown_read[] = {0x37};
    fixture.m_rx_buffer.push(unknown_read, unknown_read + std::size(unknown_read));
    battery.handle_receive(std::size(unknown_read));
    battery.handle_request();

    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x09, fixture.pop_tx(), "observed: the 0x37 request is answered with Voltage(); specified: filler for a command without an active read");
}

/// Issue #12: OBSERVED a Write Word frame with a byte beyond the word is accepted; the extra byte is drained silently.
void test_issue_12_word_write_with_trailing_byte_is_accepted()
{
    TestFixture fixture;
    TestFixture::Battery battery(fixture.m_wire, reply_zero, capture_write);
    const std::uint8_t frame[] = {0x03, 0x01, 0x02, 0x03};
    fixture.m_rx_buffer.push(frame, frame + std::size(frame));
    const auto result = battery.handle_receive(std::size(frame));

    TEST_ASSERT_TRUE_MESSAGE(result.has_value(), "observed: a 3-byte payload to a word command is accepted; SMBus §7.7 lists an incorrect data size as an error");
}

/// Issue #12: OBSERVED a Write Block frame carrying more data bytes than its count is accepted.
void test_issue_12_block_write_count_below_payload_is_accepted()
{
    TestFixture fixture;
    TestFixture::Battery battery(fixture.m_wire, reply_zero, capture_write);
    const std::uint8_t frame[] = {static_cast<std::uint8_t>(sfg_sbs::Command::OptionalMfgFunction5), 0x01, 0xAA, 0xBB, 0xCC};
    fixture.m_rx_buffer.push(frame, frame + std::size(frame));
    const auto result = battery.handle_receive(std::size(frame));

    TEST_ASSERT_TRUE_MESSAGE(result.has_value(), "observed: count 1 with 3 data bytes is accepted; Appendix C requires BadSize (0x0006)");
}

/// Issue #12: OBSERVED a Write Block frame with a zero count is accepted.
void test_issue_12_block_write_with_zero_count_is_accepted()
{
    TestFixture fixture;
    TestFixture::Battery battery(fixture.m_wire, reply_zero, capture_write);
    const std::uint8_t frame[] = {static_cast<std::uint8_t>(sfg_sbs::Command::OptionalMfgFunction5), 0x00};
    fixture.m_rx_buffer.push(frame, frame + std::size(frame));
    const auto result = battery.handle_receive(std::size(frame));

    TEST_ASSERT_TRUE_MESSAGE(result.has_value(), "observed: a zero count is accepted; SMBus §7.5.7: the byte count may not be 0");
}

/// Issue #12: OBSERVED the host sends an empty block, i.e. a zero count.
void test_issue_12_host_accepts_empty_block_write()
{
    TestFixture fixture;
    TestFixture::Host host(fixture.m_wire);
    const auto result = host.write<sfg_sbs::Command::OptionalMfgFunction5>(sfg_sbs::Block());

    TEST_ASSERT_TRUE_MESSAGE(result.has_value(), "observed: an empty block is written with count 0; SMBus §7.5.7: the byte count may not be 0");
}

/// Issue #12: OBSERVED ManufacturerData() defaults to an empty block, which a battery answers with a zero count.
void test_issue_12_manufacturer_data_default_is_empty_block()
{
    const auto value = sfg_sbs::get_default_value(sfg_sbs::Command::ManufacturerData);

    TEST_ASSERT_EQUAL_UINT_MESSAGE(0, etl::get<sfg_sbs::Block>(value).size(), "observed: empty default; SMBus §7.5.7: the byte count may not be 0");
}

/// Issue #14: OBSERVED the read handler returns a bare value, so the application cannot report an Appendix C error.
void test_issue_14_read_handler_has_no_error_channel()
{
    TEST_ASSERT_TRUE_MESSAGE((std::is_same_v<std::invoke_result_t<sfg_sbs::ReadHandler, sfg_sbs::Command>, sfg_sbs::Value>), "observed: ReadHandler returns Value; Busy, Overflow/Underflow and UnsupportedCommand have no path");
}

/// Issue #14: OBSERVED the write handler returns nothing, so the application cannot refuse a written value.
void test_issue_14_write_handler_has_no_error_channel()
{
    TEST_ASSERT_TRUE_MESSAGE((std::is_same_v<std::invoke_result_t<sfg_sbs::WriteHandler, sfg_sbs::Command, const sfg_sbs::Value &>, void>), "observed: WriteHandler returns void; Busy and Overflow/Underflow have no path");
}

/// @brief Unity hook executed before each test case.
void setUp()
{
    s_write_called = false;
    s_write_command = sfg_sbs::Command::Voltage;
    s_written_value = sfg_sbs::Value{sfg_sbs::Word(static_cast<std::uint16_t>(0))};
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
    RUN_TEST(test_issue_1_transaction_error_leaves_battery_status_untouched);
    RUN_TEST(test_issue_1_oversized_block_count_reports_no_bad_size);
    RUN_TEST(test_issue_2_broadcast_ignores_battery_mode_bits);
    RUN_TEST(test_issue_3_broadcast_repeats_without_cadence);
    RUN_TEST(test_issue_3_no_on_state_suppression_window);
    RUN_TEST(test_issue_4_no_charging_broadcast_api);
    RUN_TEST(test_issue_5_request_of_unknown_command_emits_filler);
    RUN_TEST(test_issue_6_battery_mode_write_is_unvalidated);
    RUN_TEST(test_issue_7_host_read_returns_error_nibble_verbatim);
    RUN_TEST(test_issue_9_host_has_no_broadcast_receive_api);
    RUN_TEST(test_issue_9_battery_path_refuses_a_broadcast_frame);
    RUN_TEST(test_issue_10_specification_info_default_is_not_v1_1);
    RUN_TEST(test_issue_11_orphaned_read_answers_next_unknown_request);
    RUN_TEST(test_issue_12_word_write_with_trailing_byte_is_accepted);
    RUN_TEST(test_issue_12_block_write_count_below_payload_is_accepted);
    RUN_TEST(test_issue_12_block_write_with_zero_count_is_accepted);
    RUN_TEST(test_issue_12_host_accepts_empty_block_write);
    RUN_TEST(test_issue_12_manufacturer_data_default_is_empty_block);
    RUN_TEST(test_issue_13_broadcast_command_byte_is_hardcoded);
    RUN_TEST(test_issue_14_read_handler_has_no_error_channel);
    RUN_TEST(test_issue_14_write_handler_has_no_error_channel);
    return UNITY_END();
}
