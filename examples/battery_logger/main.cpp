#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wc++20-extensions"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wvariadic-macros"
#include <Arduino.h>
#pragma GCC diagnostic pop

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wpedantic"
#include <etl/circular_buffer.h>
#pragma GCC diagnostic pop

#include <magic_enum.hpp>
#include <Wire.h>

#include "sfg_sbs/sbs.hpp"

enum class EventType
{
    HostRead,
    HostWrite,
    ReceiveError,
    RequestError
};

struct Event
{
public:
    std::uint32_t m_timestamp;
    EventType m_type;
    sfg_sbs::Command m_command;
    etl::expected<sfg_sbs::Value, sfg_sbs::Error> m_value;
};

etl::circular_buffer<Event, 32> event_buffer;

#ifdef PICO
TwoWire &battery_wire = Wire;
#else
TwoWire battery_wire = TwoWire(PC1, PC0);
#endif

sfg_sbs::Value handle_host_read(sfg_sbs::Command);
void handle_host_write(sfg_sbs::Command, const sfg_sbs::Value &);
sfg_sbs::Battery<TwoWire> battery = sfg_sbs::Battery(battery_wire, handle_host_read, handle_host_write);

sfg_sbs::Value handle_host_read(sfg_sbs::Command command)
{
    sfg_sbs::Value value;

    switch (command)
    {
        case sfg_sbs::Command::Voltage:
        {
            value = sfg_sbs::Word(static_cast<std::uint16_t>(16800));
            break;
        }
        case sfg_sbs::Command::Current:
        case sfg_sbs::Command::AverageCurrent:
        {
            value = sfg_sbs::Word(static_cast<std::int16_t>(0));
            break;
        }
        case sfg_sbs::Command::MaxError:
        {
            value = sfg_sbs::Word(static_cast<std::uint16_t>(0));
            break;
        }
        case sfg_sbs::Command::RelativeStateOfCharge:
        {
            value = sfg_sbs::Word(static_cast<std::uint16_t>(100));
            break;
        }
        case sfg_sbs::Command::RemainingCapacity:
        {
            value = sfg_sbs::Word(static_cast<std::uint16_t>(6000));
            break;
        }
        case sfg_sbs::Command::FullChargeCapacity:
        case sfg_sbs::Command::DesignCapacity:
        {
            value = sfg_sbs::Word(static_cast<std::uint16_t>(8000));
            break;
        }
        case sfg_sbs::Command::ChargingVoltage:
        {
            value = sfg_sbs::Word(static_cast<std::uint16_t>(16800));
            break;
        }
        case sfg_sbs::Command::CycleCount:
        {
            value = sfg_sbs::Word(static_cast<std::uint16_t>(10));
            break;
        }
        default:
        {
            value = sfg_sbs::get_default_value(command);
            break;
        }
    }
    event_buffer.push({millis(), EventType::HostRead, command, value});
    return value;
}

void handle_host_write(sfg_sbs::Command command, const sfg_sbs::Value &value)
{
    event_buffer.push({millis(), EventType::HostWrite, command, value});
}

void on_receive(int bytes_received)
{
    if (auto result = battery.handle_receive(bytes_received); !result.has_value())
    {
        event_buffer.push({millis(), EventType::ReceiveError, static_cast<sfg_sbs::Command>(0), etl::unexpected(result.error())});
    }
}

void on_request()
{
    if (auto result = battery.handle_request(); !result.has_value())
    {
        event_buffer.push({millis(), EventType::RequestError, static_cast<sfg_sbs::Command>(0), etl::unexpected(result.error())});
    }
}

void setup()
{
    battery_wire.begin(0x0B);
    battery_wire.onReceive(on_receive);
    battery_wire.onRequest(on_request);
    Serial.begin(115200);
    delay(1000);
}

void loop()
{
    if (event_buffer.empty())
    {
        static std::uint32_t last_print_time_ms = millis();
        std::uint32_t print_interval_ms = 1000;

        if (millis() - last_print_time_ms >= print_interval_ms)
        {
            Serial.printf("[%lu] [No events]\n", millis());
            last_print_time_ms = millis();
        }
        return;
    }

    noInterrupts();
    auto event = event_buffer.front();
    event_buffer.pop();
    interrupts();

    Serial.printf(
        "[%lu] [%s]",
        event.m_timestamp,
        magic_enum::enum_name(event.m_type).data());

    if (event.m_value.has_value())
    {
        Serial.printf(" [%s]", magic_enum::enum_name(event.m_command).data());
        const auto traits = sfg_sbs::get_runtime_traits(event.m_command).value();

        switch (traits.m_protocol)
        {
            case sfg_sbs::SmbusProtocol::Word:
            {
                Serial.printf(" 0x%04X", etl::get<sfg_sbs::Word>(event.m_value.value()).as_uint());

                switch (traits.m_data_type)
                {
                    case sfg_sbs::DataType::Int:
                    {
                        Serial.printf(" %d", etl::get<sfg_sbs::Word>(event.m_value.value()).as_int());
                        break;
                    }
                    case sfg_sbs::DataType::UnsignedInt:
                    case sfg_sbs::DataType::Word:
                    {
                        Serial.printf(" %u", etl::get<sfg_sbs::Word>(event.m_value.value()).as_uint());
                        break;
                    }
                    case sfg_sbs::DataType::Boolean:
                    {
                        Serial.printf(" %s", etl::get<sfg_sbs::Word>(event.m_value.value()).as_bool() ? "true" : "false");
                        break;
                    }
                    default:
                    {
                        break;
                    }
                }
                break;
            }
            case sfg_sbs::SmbusProtocol::Block:
            {
                Serial.printf(" %s", etl::get<sfg_sbs::Block>(event.m_value.value()).as_string().data());
                break;
            }
        }
    }
    else
    {
        Serial.printf(" [%lu] %s", event.m_command, magic_enum::enum_name(event.m_value.error()).data());
    }
    Serial.println();
}