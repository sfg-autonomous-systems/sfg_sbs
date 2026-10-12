#pragma once

#include <cstdint>

namespace sfg_sbs
{
    /// @brief Bit layout of BatteryStatus() as specified in Smart Battery Data Specification Rev 1.1, §5.1.21.
    struct BatteryStatus
    {
    public:
        /// @brief Error code in bits 0-3. Compared, never combined.
        enum class ErrorCode : std::uint16_t
        {
            Ok,
            Busy,
            ReservedCommand,
            UnsupportedCommand,
            AccessDenied,
            OverflowUnderflow,
            BadSize,
            UnknownError
        };

        /// @brief Status bits. Unscoped so they combine with the raw word without casts.
        enum StatusBits : std::uint16_t
        {
            FullyDischarged = 1 << 4,
            FullyCharged = 1 << 5,
            Discharging = 1 << 6,
            Initialized = 1 << 7
        };

        /// @brief Alarm bits. Unscoped so they combine with the raw word without casts.
        enum AlarmBits : std::uint16_t
        {
            RemainingTimeAlarm = 1 << 8,
            RemainingCapacityAlarm = 1 << 9,
            TerminateDischargeAlarm = 1 << 11,
            OverTempAlarm = 1 << 12,
            TerminateChargeAlarm = 1 << 14,
            OverChargedAlarm = 1 << 15
        };

        static constexpr std::uint16_t s_error_code_mask = 0x000F;
        static constexpr std::uint16_t s_status_bits_mask = FullyDischarged | FullyCharged | Discharging | Initialized;
        static constexpr std::uint16_t s_alarm_bits_mask = RemainingTimeAlarm | RemainingCapacityAlarm | TerminateDischargeAlarm | OverTempAlarm | TerminateChargeAlarm | OverChargedAlarm;
        static constexpr std::uint16_t s_reserved_bits_mask = (1 << 10) | (1 << 13);

        /// @brief Not instantiable, this struct only scopes the layout constants.
        /// @note The explicit is required: in C++17 a struct with only a deleted default constructor is still an
        ///       aggregate, so BatteryStatus{} would compile without it.
        explicit BatteryStatus() = delete;
    };

    static_assert((BatteryStatus::s_error_code_mask | BatteryStatus::s_status_bits_mask | BatteryStatus::s_alarm_bits_mask | BatteryStatus::s_reserved_bits_mask) == 0xFFFF);
    static_assert((BatteryStatus::s_error_code_mask ^ BatteryStatus::s_status_bits_mask ^ BatteryStatus::s_alarm_bits_mask ^ BatteryStatus::s_reserved_bits_mask) == 0xFFFF);
}
