#include <algorithm>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wpedantic"
#include <Arduino.h>
#pragma GCC diagnostic pop

#include <etl/unordered_map.h>
#include <iterator>
#include <limits>
#include <numeric>
#include <STM32LowPower.h>
#include <Wire.h>

#include "sfg_sbs/sbs.hpp"

constexpr std::uint8_t smbus_host_address = 0x08;
constexpr std::uint8_t smart_charger_address = 0x09;
constexpr std::uint32_t poll_interval_ms = 3000;
constexpr size_t battery_count = 2;

/// @brief Battery has no repetition timer. It broadcasts whenever the application polls, so the
/// poll rate is what satisfies SBS rev 1.1 §5.4.1's 10 second interval.
static_assert(poll_interval_ms <= 10000, "AlarmWarning() must reach every peer at least once per 10s (§5.4.1).");

TwoWire host_wires[battery_count] = {
    TwoWire(PB9, PB8),
    TwoWire(PB14, PB13),
};
sfg_sbs::Host<TwoWire> hosts[battery_count] = {
    sfg_sbs::Host(host_wires[0]),
    sfg_sbs::Host(host_wires[1]),
};

etl::unordered_map<sfg_sbs::Command, sfg_sbs::Value, sfg_sbs::s_command_count> fused_values;

TwoWire battery_wire(PC1, PC0);
sfg_sbs::Battery<TwoWire> battery(
    battery_wire,
    [](sfg_sbs::Command command)
    {
        auto iterator = fused_values.find(command);

        if (iterator == fused_values.end())
        {
            return sfg_sbs::get_default_value(command);
        }
        return iterator->second;
    },
    nullptr,
    smbus_host_address,
    smart_charger_address);

template <sfg_sbs::Command Command, typename TFuseFunction>
void poll_and_fuse(TFuseFunction fuse)
{
    using NativeType = typename sfg_sbs::CommandTraits<Command>::NativeType;
    NativeType values[battery_count];

    for (size_t index = 0; index < battery_count; index++)
    {
        auto result = hosts[index].read<Command>();

        if (!result.has_value())
        {
            host_wires[index].begin();
            return;
        }
        values[index] = result.value();
    }
    sfg_sbs::Value fused_value = sfg_sbs::Word(fuse(values));

    noInterrupts();
    fused_values[Command] = fused_value;
    interrupts();
}

template <typename TNative>
TNative sum(const TNative (&values)[battery_count])
{
    constexpr std::int32_t min_value = std::numeric_limits<TNative>::min();
    constexpr std::int32_t max_value = std::numeric_limits<TNative>::max();

    return static_cast<TNative>(std::clamp(std::accumulate(std::begin(values), std::end(values), static_cast<std::int32_t>(0)), min_value, max_value));
}

template <typename TNative>
TNative average(const TNative (&values)[battery_count])
{
    return static_cast<TNative>(std::accumulate(std::begin(values), std::end(values), static_cast<std::int32_t>(0)) / static_cast<std::int32_t>(battery_count));
}

void on_host_receive(int bytes_received)
{
    battery.handle_receive(static_cast<size_t>(bytes_received));
}

void on_host_request()
{
    battery.handle_request();
}

void setup()
{
    for (auto &wire : host_wires)
    {
        wire.begin();
    }

    battery_wire.begin(0x0B);
    battery_wire.onReceive(on_host_receive);
    battery_wire.onRequest(on_host_request);

    RCC_PeriphCLKInitTypeDef rcc_peripheral_clock_init = {};
    rcc_peripheral_clock_init.PeriphClockSelection = RCC_PERIPHCLK_I2C3;
    rcc_peripheral_clock_init.I2c3ClockSelection = RCC_I2C3CLKSOURCE_HSI;
    HAL_RCCEx_PeriphCLKConfig(&rcc_peripheral_clock_init);
    HAL_I2CEx_EnableWakeUp(battery_wire.getHandle());

    LowPower.begin();
}

void loop()
{
    poll_and_fuse<sfg_sbs::Command::BatteryStatus>(
        [](const std::uint16_t (&values)[battery_count])
        {
            std::uint16_t battery_status = etl::get<sfg_sbs::Word>(sfg_sbs::get_default_value(sfg_sbs::Command::BatteryStatus)).as_uint();

            for (auto value : values)
            {
                battery_status |= value & sfg_sbs::BatteryStatus::s_alarm_bits_mask;
            }
            battery.broadcast_alarm_warning_if_required(battery_status);
            return battery_status;
        });
    poll_and_fuse<sfg_sbs::Command::Current>(sum<std::int16_t>);
    poll_and_fuse<sfg_sbs::Command::RelativeStateOfCharge>(average<std::uint16_t>);
    poll_and_fuse<sfg_sbs::Command::Voltage>(average<std::uint16_t>);

    // We put the uC into sleep until we poll the batteries again. Because the uC
    // might also wake up due to an application request, and afterwards happily
    // continue executing the loop, effectively butchering our sleep, we need
    // to account for this scenario and actually check if we slept for the full
    // duration, and if not, sleep for the remaining time.
    std::uint32_t start_sleep_ms = millis();

    while (millis() - start_sleep_ms < poll_interval_ms)
    {
        LowPower.deepSleep(poll_interval_ms - (millis() - start_sleep_ms));
    }
}
