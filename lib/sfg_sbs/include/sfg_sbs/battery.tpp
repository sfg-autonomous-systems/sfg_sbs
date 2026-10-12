#include "sfg_sbs/battery.hpp"

#include "sfg_sbs/command_runtime_traits.hpp"
#include "sfg_sbs/detail/transport.hpp"
#include "sfg_sbs/utils/scope_guard.hpp"

namespace sfg_sbs
{
    template <typename TBus>
    Battery<TBus>::Battery(
        TBus &bus,
        ReadHandler read_handler,
        WriteHandler write_handler,
        std::optional<std::uint8_t> host_address,
        std::optional<std::uint8_t> charger_address)
        : m_bus(bus),
          m_read_handler(read_handler),
          m_write_handler(write_handler),
          m_host_address(host_address),
          m_charger_address(charger_address)
    {
    }

    template <typename TBus>
    etl::expected<void, Error> Battery<TBus>::handle_receive(size_t bytes_received)
    {
        if (bytes_received == 0)
        {
            return etl::unexpected(Error::NoDataAvailable);
        }

        utils::ScopeGuard drain_bus(
            [this]()
            {
                while (m_bus.available())
                {
                    m_bus.read();
                }
            });

        auto byte = m_bus.read();

        // If the byte is negative, the bus has no data available. Technically this should
        // never happen because then the caller would have supplied 0 for bytes_received
        // but it is a defensive measure.
        if (byte < 0)
        {
            return etl::unexpected(Error::NoDataAvailable);
        }

        auto command = static_cast<Command>(byte);
        const auto traits = get_runtime_traits(command);

        if (!traits.has_value())
        {
            return etl::unexpected(Error::UnknownCommand);
        }
        m_active_command = command;

        // If no bytes remain, it's a read request, so we can return early.
        if (--bytes_received == 0)
        {
            return {};
        }

        utils::ScopeGuard reset_active_command(
            [this]()
            {
                m_active_command.reset();
            });
        Value value;

        if (traits.value().m_access_type == AccessType::ReadOnly)
        {
            return etl::unexpected(Error::AccessDenied);
        }

        if (auto result = detail::consume_value(m_bus, value, traits.value()); !result.has_value())
        {
            return etl::unexpected(result.error());
        }

        if (!m_write_handler)
        {
            return etl::unexpected(Error::NoWriteHandlerBound);
        }
        // Route the data to the application if the handler is bound.
        m_write_handler(m_active_command.value(), value);
        return {};
    }

    template <typename TBus>
    etl::expected<void, Error> Battery<TBus>::handle_request()
    {
        // Every exit path fills the TX buffer up to the command's maximum wire size.
        // Some hosts (e.g. the Turtlebot 4) always clock out a full 33-byte block read
        // regardless of the command, and an STM32 Wire slave misbehaves if its TX buffer
        // runs dry or is empty during a read. A host that follows the specification reads
        // only the word or the announced block length, so the extra bytes are never
        // clocked out and the padding is invisible on the wire.
        utils::ScopeGuard reset_active_command(
            [this]()
            {
                m_active_command.reset();
            });

        if (!m_read_handler)
        {
            detail::emit_raw(m_bus, static_cast<std::uint8_t>(0x00), Value::s_max_wire_size);
            return etl::unexpected(Error::NoReadHandlerBound);
        }

        if (!m_active_command.has_value())
        {
            detail::emit_raw(m_bus, static_cast<std::uint8_t>(0x00), Value::s_max_wire_size);
            return etl::unexpected(Error::NoActiveCommand);
        }

        const auto traits = get_runtime_traits(m_active_command.value());
        const auto value = m_read_handler(m_active_command.value());
        auto result = detail::emit_value(m_bus, value, traits.value());

        if (!result.has_value())
        {
            detail::emit_raw(m_bus, static_cast<std::uint8_t>(0x00), traits.value().m_max_wire_size);
            return etl::unexpected(result.error());
        }
        // Pad to the maximum wire size, see the note at the top of this function.
        detail::emit_raw(m_bus, static_cast<std::uint8_t>(0x00), traits.value().m_max_wire_size - result.value());
        return {};
    }

    template <typename TBus>
    etl::expected<bool, Error> Battery<TBus>::broadcast_alarm_warning_if_required(std::uint16_t battery_status)
    {
        auto broadcast_alarm_warning_to_host = broadcast_alarm_warning_if_required(AlarmWarningTarget::Host, battery_status);
        auto broadcast_alarm_warning_to_charger = broadcast_alarm_warning_if_required(AlarmWarningTarget::Charger, battery_status);

        if (!broadcast_alarm_warning_to_charger.has_value())
        {
            return etl::unexpected(broadcast_alarm_warning_to_charger.error());
        }

        if (!broadcast_alarm_warning_to_host.has_value())
        {
            return etl::unexpected(broadcast_alarm_warning_to_host.error());
        }

        return broadcast_alarm_warning_to_host.value() || broadcast_alarm_warning_to_charger.value();
    }

    template <typename TBus>
    etl::expected<bool, Error> Battery<TBus>::broadcast_alarm_warning_if_required(AlarmWarningTarget target, std::uint16_t battery_status)
    {
        bool should_broadcast_alarm_warning = false;
        std::optional<std::uint8_t> target_address = std::nullopt;

        switch (target)
        {
            case AlarmWarningTarget::Host:
            {
                should_broadcast_alarm_warning = (battery_status & s_battery_status_alarm_bits_mask_host) != 0;
                target_address = m_host_address;
                break;
            }
            case AlarmWarningTarget::Charger:
            {
                should_broadcast_alarm_warning = (battery_status & s_battery_status_alarm_bits_mask_charger) != 0;
                target_address = m_charger_address;
                break;
            }
        }

        if (!should_broadcast_alarm_warning)
        {
            return false;
        }

        if (!target_address.has_value())
        {
            return false;
        }

        auto command = Command::BatteryStatus;
        auto word = Word(static_cast<std::uint16_t>(battery_status | BatteryStatus::s_error_code_mask));

        // Technically Command::BatteryStatus (0x16) is ReadOnly, but this is actual implementing AlarmWarning() routing.
        // Because Smart Battery Data Specification Rev 1.1, §5.4.1 uses the same command code (0x16) and Command is an
        // enum class which cannot have duplicate values, we are using Command::BatteryStatus to send the AlarmWarning().
        if (auto result = detail::write_word(m_bus, command, word, get_runtime_traits(command).value(), target_address.value()); !result.has_value())
        {
            return etl::unexpected(result.error());
        }
        return true;
    }
}