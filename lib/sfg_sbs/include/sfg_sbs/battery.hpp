#pragma once

#include <cstddef>
#include <cstdint>
#include <etl/expected.h>
#include <functional>
#include <optional>

#include "sfg_sbs/battery_status.hpp"
#include "sfg_sbs/command.hpp"
#include "sfg_sbs/error.hpp"
#include "sfg_sbs/value.hpp"

namespace sfg_sbs
{
    using ReadHandler = std::function<Value(Command)>;
    using WriteHandler = std::function<void(Command, const Value &)>;

    template <typename TBus>
    class Battery
    {
    public:
        explicit Battery(
            TBus &bus,
            ReadHandler read_handler,
            WriteHandler write_handler,
            std::optional<std::uint8_t> host_address = std::nullopt,
            std::optional<std::uint8_t> charger_address = std::nullopt);

        etl::expected<void, Error> handle_receive(size_t bytes_received);
        etl::expected<void, Error> handle_request();
        etl::expected<bool, Error> broadcast_alarm_warning_if_required(std::uint16_t battery_status);

    private:
        enum class AlarmWarningTarget
        {
            Host,
            Charger
        };

        etl::expected<bool, Error> broadcast_alarm_warning_if_required(AlarmWarningTarget target, std::uint16_t battery_status);

        // AlarmWarning() routing, spec rev 1.1 section 5.4.1. The rule is written twice and the
        // two statements disagree for TERMINATE_DISCHARGE_ALARM: p.40 restricts charger
        // delivery to "alarms relating to charging", the note on p.41 sends every alarm bit except
        // REMAINING_CAPACITY_ALARM and REMAINING_TIME_ALARM to both peers.
        //
        // Decision:
        //     Follow p.41, so TERMINATE_DISCHARGE_ALARM is sent to both host and charger because
        //     it is the safest option. Worst case, the charger will ignore it.
        static constexpr std::uint16_t s_battery_status_alarm_bits_mask_host = BatteryStatus::s_alarm_bits_mask;
        static constexpr std::uint16_t s_battery_status_alarm_bits_mask_charger =
            BatteryStatus::OverChargedAlarm |
            BatteryStatus::TerminateChargeAlarm |
            BatteryStatus::OverTempAlarm |
            BatteryStatus::TerminateDischargeAlarm;

        TBus &m_bus;
        ReadHandler m_read_handler;
        WriteHandler m_write_handler;
        std::optional<Command> m_active_command;
        std::optional<std::uint8_t> m_host_address;
        std::optional<std::uint8_t> m_charger_address;
    };
}

#include "sfg_sbs/battery.tpp"