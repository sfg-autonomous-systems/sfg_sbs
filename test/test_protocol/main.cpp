/// @file main.cpp
/// @brief Protocol suite: the byte-level contract of the SBS/SMBus transfer paths, including block size limits
///        and the padding of battery replies.
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <unity.h>

#include "test_fixture.hpp"

/// @brief SMBus address of the Smart Battery, which a default-constructed host talks to.
static constexpr std::uint8_t s_battery_address = 0x0B;

/// @brief Records what the battery's write handler received, so decoding and routing are assertable.
static bool s_write_called = false;
static sfg_sbs::Command s_write_command = sfg_sbs::Command::Voltage;
static sfg_sbs::Value s_written_value{};

/// @brief A write handler capturing the routed command and value.
static void capture_write(sfg_sbs::Command command, const sfg_sbs::Value &value)
{
    s_write_called = true;
    s_write_command = command;
    s_written_value = value;
}

/// @brief Answers word commands with 0x1234 and block commands with "Hi", so reply bytes are known in advance.
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

/// @brief Queues the word reply 0x1234 for the host, low byte first.
static void push_word_reply(TestFixture &fixture)
{
    const std::uint8_t data[] = {0x34, 0x12};
    fixture.m_rx_buffer.push(data, data + std::size(data));
}

/// @brief Queues a block reply for the host, i.e. a count byte followed by @p count 'a' characters.
static void push_block_reply(TestFixture &fixture, std::uint8_t count)
{
    fixture.m_rx_buffer.push(count);

    for (size_t index = 0; index < count; index++)
    {
        fixture.m_rx_buffer.push(static_cast<std::uint8_t>('a'));
    }
}

/// @brief Queues a Block Write to OptionalMfgFunction5(), the only writable block command, carrying
///        a count byte of @p count followed by @p count 'a' characters.
/// @return The number of bytes queued.
static size_t push_block_write(TestFixture &fixture, std::uint8_t count)
{
    fixture.m_rx_buffer.push(static_cast<std::uint8_t>(sfg_sbs::Command::OptionalMfgFunction5));
    push_block_reply(fixture, count);
    return static_cast<size_t>(count) + 2;
}

/// @brief Arms ManufacturerName() on the battery and lets it answer the request phase.
static void request_block_reply(TestFixture &fixture, TestFixture::Battery &battery)
{
    const std::uint8_t data[] = {static_cast<std::uint8_t>(sfg_sbs::Command::ManufacturerName)};
    fixture.m_rx_buffer.push(data, data + std::size(data));
    battery.handle_receive(std::size(data));
    battery.handle_request();
}

/// @brief A Read Word reply must be reassembled from the peer's bytes low byte first.
void test_host_read_word_is_little_endian()
{
    TestFixture fixture;
    TestFixture::Host host(fixture.m_wire);
    const std::uint8_t data[] = {0x34, 0x12};
    fixture.m_rx_buffer.push(data, data + std::size(data));
    const auto value = host.read<sfg_sbs::Command::Voltage>();

    TEST_ASSERT_TRUE(value.has_value());
    TEST_ASSERT_EQUAL_UINT16(0x1234, value.value());
}

/// @brief A signed Read Word reply must arrive as a negative number, not as its unsigned bit pattern.
void test_host_read_word_preserves_sign()
{
    TestFixture fixture;
    TestFixture::Host host(fixture.m_wire);
    const std::int16_t data = -500;
    fixture.m_rx_buffer.push(reinterpret_cast<const std::uint8_t *>(&data), reinterpret_cast<const std::uint8_t *>(&data) + sizeof(data));
    const auto value = host.read<sfg_sbs::Command::Current>();

    TEST_ASSERT_TRUE(value.has_value());
    TEST_ASSERT_EQUAL_INT16(-500, value.value());
}

/// @brief A Block Read must consume the count byte and reproduce the peer's payload byte for byte.
void test_host_read_block_strips_count_byte()
{
    TestFixture fixture;
    TestFixture::Host host(fixture.m_wire);
    const std::uint8_t data[] = {0x02, 'H', 'i'};
    fixture.m_rx_buffer.push(data, data + std::size(data));
    const auto value = host.read<sfg_sbs::Command::ManufacturerName>();

    TEST_ASSERT_TRUE(value.has_value());
    TEST_ASSERT_EQUAL_UINT8(2, value->length());
    TEST_ASSERT_EQUAL_STRING("Hi", value->data());
}

/// @brief The host accepts a Block Read carrying the maximum data size (Appendix B).
void test_host_accepts_block_read_of_max_data_size()
{
    TestFixture fixture;
    TestFixture::Host host(fixture.m_wire);
    push_block_reply(fixture, sfg_sbs::Block::s_max_data_size);
    const auto value = host.read<sfg_sbs::Command::ManufacturerName>();

    TEST_ASSERT_TRUE(value.has_value());
    TEST_ASSERT_EQUAL_UINT8(sfg_sbs::Block::s_max_data_size, value->length());
}

/// @brief The host refuses a Block Read whose count byte exceeds the maximum data size with BadSize.
void test_host_rejects_block_read_over_max_data_size()
{
    TestFixture fixture;
    TestFixture::Host host(fixture.m_wire);
    push_block_reply(fixture, sfg_sbs::Block::s_max_data_size + 1);
    const auto value = host.read<sfg_sbs::Command::ManufacturerName>();

    TEST_ASSERT_FALSE(value.has_value());

    if (!value.has_value())
    {
        TEST_ASSERT_TRUE_MESSAGE(value.error() == sfg_sbs::Error::BadSize, "The count byte should be refused with BadSize.");
    }
}

/// @brief A Write Word puts the command code, the low byte and the high byte on the bus, and nothing else.
void test_host_write_word_is_command_low_high()
{
    TestFixture fixture;
    TestFixture::Host host(fixture.m_wire);

    TEST_ASSERT_TRUE(host.write<sfg_sbs::Command::BatteryMode>(0x1234).has_value());
    TEST_ASSERT_EQUAL_UINT(3, fixture.m_tx_buffer.size());
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(sfg_sbs::Command::BatteryMode), fixture.pop_tx());
    TEST_ASSERT_EQUAL_UINT8(0x34, fixture.pop_tx());
    TEST_ASSERT_EQUAL_UINT8(0x12, fixture.pop_tx());
}

/// @brief A Block Write puts the command code, the count byte and the payload on the bus, and nothing else.
void test_host_write_block_is_command_count_payload()
{
    TestFixture fixture;
    TestFixture::Host host(fixture.m_wire);

    TEST_ASSERT_TRUE(host.write<sfg_sbs::Command::OptionalMfgFunction5>(sfg_sbs::Block("Hi")).has_value());
    TEST_ASSERT_EQUAL_UINT(4, fixture.m_tx_buffer.size());
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(sfg_sbs::Command::OptionalMfgFunction5), fixture.pop_tx());
    TEST_ASSERT_EQUAL_UINT8(0x02, fixture.pop_tx());
    TEST_ASSERT_EQUAL_UINT8('H', fixture.pop_tx());
    TEST_ASSERT_EQUAL_UINT8('i', fixture.pop_tx());
}

/// @brief A host write addresses the battery.
void test_host_write_addresses_battery()
{
    TestFixture fixture;
    TestFixture::Host host(fixture.m_wire);
    host.write<sfg_sbs::Command::BatteryMode>(0x1234);

    TEST_ASSERT_EQUAL_HEX8(s_battery_address, fixture.get_invocation<TestFixture::Wire::BeginTransmission>().m_address);
}

/// @brief A host write closes its transaction with a stop bit.
void test_host_write_ends_with_stop()
{
    TestFixture fixture;
    TestFixture::Host host(fixture.m_wire);
    host.write<sfg_sbs::Command::BatteryMode>(0x1234);

    TEST_ASSERT_TRUE(fixture.get_invocation<TestFixture::Wire::EndTransmission>().m_stop_bit);
}

/// @brief A host read addresses the battery in its command phase.
void test_host_read_addresses_battery()
{
    TestFixture fixture;
    TestFixture::Host host(fixture.m_wire);
    push_word_reply(fixture);
    host.read<sfg_sbs::Command::Voltage>();

    TEST_ASSERT_EQUAL_HEX8(s_battery_address, fixture.get_invocation<TestFixture::Wire::BeginTransmission>().m_address);
}

/// @brief A host read writes the command code in its command phase.
void test_host_read_writes_command_code()
{
    TestFixture fixture;
    TestFixture::Host host(fixture.m_wire);
    push_word_reply(fixture);
    host.read<sfg_sbs::Command::Voltage>();

    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(sfg_sbs::Command::Voltage), fixture.pop_tx());
}

/// @brief A host read writes nothing but the command code.
void test_host_read_writes_one_byte()
{
    TestFixture fixture;
    TestFixture::Host host(fixture.m_wire);
    push_word_reply(fixture);
    host.read<sfg_sbs::Command::Voltage>();

    TEST_ASSERT_EQUAL_UINT(1, fixture.m_tx_buffer.size());
}

/// @brief A host read ends its command phase without a stop bit, so the reply follows as a repeated start.
void test_host_read_ends_command_phase_without_stop()
{
    TestFixture fixture;
    TestFixture::Host host(fixture.m_wire);
    push_word_reply(fixture);
    host.read<sfg_sbs::Command::Voltage>();

    TEST_ASSERT_FALSE(fixture.get_invocation<TestFixture::Wire::EndTransmission>().m_stop_bit);
}

/// @brief A host read requests the reply only after its command phase.
void test_host_read_requests_reply_after_command_phase()
{
    TestFixture fixture;
    TestFixture::Host host(fixture.m_wire);
    push_word_reply(fixture);
    host.read<sfg_sbs::Command::Voltage>();

    TEST_ASSERT_GREATER_THAN_UINT(
        fixture.get_invocation_index<TestFixture::Wire::EndTransmission>(),
        fixture.get_invocation_index<TestFixture::Wire::RequestFrom>());
}

/// @brief A host read requests its reply from the battery.
void test_host_read_requests_reply_from_battery()
{
    TestFixture fixture;
    TestFixture::Host host(fixture.m_wire);
    push_word_reply(fixture);
    host.read<sfg_sbs::Command::Voltage>();

    TEST_ASSERT_EQUAL_HEX8(s_battery_address, fixture.get_invocation<TestFixture::Wire::RequestFrom>().m_address);
}

/// @brief A host read releases the bus with a stop bit after the reply.
void test_host_read_ends_reply_phase_with_stop()
{
    TestFixture fixture;
    TestFixture::Host host(fixture.m_wire);
    push_word_reply(fixture);
    host.read<sfg_sbs::Command::Voltage>();

    TEST_ASSERT_TRUE(fixture.get_invocation<TestFixture::Wire::RequestFrom>().m_stop_bit);
}

/// @brief A Read Word clocks exactly the two bytes of a word.
void test_host_read_word_requests_two_bytes()
{
    TestFixture fixture;
    TestFixture::Host host(fixture.m_wire);
    push_word_reply(fixture);
    host.read<sfg_sbs::Command::Voltage>();

    TEST_ASSERT_EQUAL_UINT(sfg_sbs::Word::s_wire_size, fixture.get_invocation<TestFixture::Wire::RequestFrom>().m_length);
}

/// @brief A Block Read clocks the maximum wire size, i.e. the count byte plus the maximum data size (Appendix B).
void test_host_read_block_requests_max_wire_size()
{
    TestFixture fixture;
    TestFixture::Host host(fixture.m_wire);
    push_block_reply(fixture, 2);
    host.read<sfg_sbs::Command::ManufacturerName>();

    TEST_ASSERT_EQUAL_UINT(sfg_sbs::Block::s_max_wire_size, fixture.get_invocation<TestFixture::Wire::RequestFrom>().m_length);
}

/// @brief A battery reply to an armed word command is exactly two bytes, low byte first, with no framing around it.
void test_battery_word_reply_is_little_endian()
{
    TestFixture fixture;
    TestFixture::Battery battery(fixture.m_wire, known_reply, nullptr);
    const std::uint8_t data[] = {static_cast<std::uint8_t>(sfg_sbs::Command::Voltage)};
    fixture.m_rx_buffer.push(data, data + std::size(data));
    battery.handle_receive(std::size(data));
    battery.handle_request();

    TEST_ASSERT_EQUAL_UINT(2, fixture.m_tx_buffer.size());
    TEST_ASSERT_EQUAL_UINT8(0x34, fixture.pop_tx());
    TEST_ASSERT_EQUAL_UINT8(0x12, fixture.pop_tx());
}

/// @brief A battery answers a request as the addressed slave and never masters the bus itself.
void test_battery_reply_does_not_master_the_bus()
{
    TestFixture fixture;
    TestFixture::Battery battery(fixture.m_wire, known_reply, nullptr);
    request_block_reply(fixture, battery);

    TEST_ASSERT_EQUAL_UINT(0, fixture.count_invocations<TestFixture::Wire::BeginTransmission>());
}

/// @brief A battery block reply starts with the count byte, followed by the payload.
void test_battery_block_reply_is_count_then_payload()
{
    TestFixture fixture;
    TestFixture::Battery battery(fixture.m_wire, known_reply, nullptr);
    request_block_reply(fixture, battery);

    TEST_ASSERT_EQUAL_UINT8(0x02, fixture.pop_tx());
    TEST_ASSERT_EQUAL_UINT8('H', fixture.pop_tx());
    TEST_ASSERT_EQUAL_UINT8('i', fixture.pop_tx());
}

/// @brief A battery block reply always fills the maximum wire size the host clocks for a Block Read.
void test_battery_block_reply_fills_max_wire_size()
{
    TestFixture fixture;
    TestFixture::Battery battery(fixture.m_wire, known_reply, nullptr);
    request_block_reply(fixture, battery);

    TEST_ASSERT_EQUAL_UINT(sfg_sbs::Block::s_max_wire_size, fixture.m_tx_buffer.size());
}

/// @brief A request with no command armed is still answered with a reply of the maximum wire size, never an empty one.
void test_battery_unarmed_request_reply_fills_max_wire_size()
{
    TestFixture fixture;
    TestFixture::Battery battery(fixture.m_wire, known_reply, nullptr);
    battery.handle_request();

    TEST_ASSERT_EQUAL_UINT(sfg_sbs::Value::s_max_wire_size, fixture.m_tx_buffer.size());
}

/// @brief A Write Word received by the battery is decoded low byte first.
void test_battery_write_word_is_little_endian()
{
    TestFixture fixture;
    TestFixture::Battery battery(fixture.m_wire, known_reply, capture_write);
    const std::uint8_t frame[] = {static_cast<std::uint8_t>(sfg_sbs::Command::BatteryMode), 0x34, 0x12};
    fixture.m_rx_buffer.push(frame, frame + std::size(frame));
    battery.handle_receive(std::size(frame));

    TEST_ASSERT_TRUE(s_write_called);
    TEST_ASSERT_EQUAL_UINT16(0x1234, etl::get<sfg_sbs::Word>(s_written_value).as_uint());
}

/// @brief A write is routed to the write handler under the command its command byte names.
void test_battery_routes_write_to_addressed_command()
{
    TestFixture fixture;
    TestFixture::Battery battery(fixture.m_wire, known_reply, capture_write);
    const std::uint8_t frame[] = {static_cast<std::uint8_t>(sfg_sbs::Command::BatteryMode), 0x34, 0x12};
    fixture.m_rx_buffer.push(frame, frame + std::size(frame));
    battery.handle_receive(std::size(frame));

    TEST_ASSERT_TRUE(s_write_called);
    TEST_ASSERT_TRUE(s_write_command == sfg_sbs::Command::BatteryMode);
}

/// @brief A Block Write carrying the maximum data size reaches the write handler with all data bytes.
void test_battery_block_write_of_max_data_size_keeps_its_length()
{
    TestFixture fixture;
    TestFixture::Battery battery(fixture.m_wire, known_reply, capture_write);
    const size_t queued = push_block_write(fixture, sfg_sbs::Block::s_max_data_size);
    battery.handle_receive(queued);

    TEST_ASSERT_TRUE(s_write_called);
    TEST_ASSERT_EQUAL_UINT8(sfg_sbs::Block::s_max_data_size, etl::get<sfg_sbs::Block>(s_written_value).size());
}

/// @brief The battery refuses a Block Write whose count byte exceeds the maximum data size with BadSize.
void test_battery_rejects_block_write_over_max_data_size()
{
    TestFixture fixture;
    TestFixture::Battery battery(fixture.m_wire, known_reply, capture_write);
    const size_t queued = push_block_write(fixture, sfg_sbs::Block::s_max_data_size + 1);
    const auto result = battery.handle_receive(queued);

    TEST_ASSERT_FALSE(result.has_value());

    if (!result.has_value())
    {
        TEST_ASSERT_TRUE_MESSAGE(result.error() == sfg_sbs::Error::BadSize, "The count byte should be refused with BadSize.");
    }
}

/// @brief A Block Write over the maximum data size never reaches the write handler.
void test_battery_does_not_route_block_write_over_max_data_size()
{
    TestFixture fixture;
    TestFixture::Battery battery(fixture.m_wire, known_reply, capture_write);
    const size_t queued = push_block_write(fixture, sfg_sbs::Block::s_max_data_size + 1);
    battery.handle_receive(queued);

    TEST_ASSERT_FALSE(s_write_called);
}

/// @brief The battery refuses a Block Write whose payload is shorter than its count byte with TooLittleDataAvailable.
void test_battery_rejects_truncated_block_write()
{
    TestFixture fixture;
    TestFixture::Battery battery(fixture.m_wire, known_reply, capture_write);
    const std::uint8_t frame[] = {static_cast<std::uint8_t>(sfg_sbs::Command::OptionalMfgFunction5), 0x05, 'a', 'b'};
    fixture.m_rx_buffer.push(frame, frame + std::size(frame));
    const auto result = battery.handle_receive(std::size(frame));

    TEST_ASSERT_FALSE(result.has_value());

    if (!result.has_value())
    {
        TEST_ASSERT_TRUE(result.error() == sfg_sbs::Error::TooLittleDataAvailable);
    }
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
    RUN_TEST(test_host_read_word_is_little_endian);
    RUN_TEST(test_host_read_word_preserves_sign);
    RUN_TEST(test_host_read_block_strips_count_byte);
    RUN_TEST(test_host_accepts_block_read_of_max_data_size);
    RUN_TEST(test_host_rejects_block_read_over_max_data_size);
    RUN_TEST(test_host_write_word_is_command_low_high);
    RUN_TEST(test_host_write_block_is_command_count_payload);
    RUN_TEST(test_host_write_addresses_battery);
    RUN_TEST(test_host_write_ends_with_stop);
    RUN_TEST(test_host_read_addresses_battery);
    RUN_TEST(test_host_read_writes_command_code);
    RUN_TEST(test_host_read_writes_one_byte);
    RUN_TEST(test_host_read_ends_command_phase_without_stop);
    RUN_TEST(test_host_read_requests_reply_after_command_phase);
    RUN_TEST(test_host_read_requests_reply_from_battery);
    RUN_TEST(test_host_read_ends_reply_phase_with_stop);
    RUN_TEST(test_host_read_word_requests_two_bytes);
    RUN_TEST(test_host_read_block_requests_max_wire_size);
    RUN_TEST(test_battery_word_reply_is_little_endian);
    RUN_TEST(test_battery_reply_does_not_master_the_bus);
    RUN_TEST(test_battery_block_reply_is_count_then_payload);
    RUN_TEST(test_battery_block_reply_fills_max_wire_size);
    RUN_TEST(test_battery_unarmed_request_reply_fills_max_wire_size);
    RUN_TEST(test_battery_write_word_is_little_endian);
    RUN_TEST(test_battery_routes_write_to_addressed_command);
    RUN_TEST(test_battery_block_write_of_max_data_size_keeps_its_length);
    RUN_TEST(test_battery_rejects_block_write_over_max_data_size);
    RUN_TEST(test_battery_does_not_route_block_write_over_max_data_size);
    RUN_TEST(test_battery_rejects_truncated_block_write);
    return UNITY_END();
}
