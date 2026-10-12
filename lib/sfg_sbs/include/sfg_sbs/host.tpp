#include "sfg_sbs/host.hpp"

#include <type_traits>

#include "sfg_sbs/command_runtime_traits.hpp"
#include "sfg_sbs/data_type.hpp"
#include "sfg_sbs/detail/transport.hpp"
#include "sfg_sbs/smbus_protocol.hpp"
#include "sfg_sbs/utils/scope_guard.hpp"
#include "sfg_sbs/word.hpp"

namespace sfg_sbs
{
    template <typename TBus>
    Host<TBus>::Host(TBus &bus, std::uint8_t battery_address)
        : m_bus(bus),
          m_battery_address(battery_address)
    {
    }

    template <typename TBus>
    template <Command Command>
    etl::expected<typename CommandTraits<Command>::NativeType, Error> Host<TBus>::read()
    {
        using Traits = CommandTraits<Command>;
        auto result = read(Command);

        if (!result.has_value())
        {
            return etl::unexpected(result.error());
        }

        const Value &value = result.value();

        if constexpr (Traits::s_data_type == DataType::Int)
        {
            return etl::get<Word>(value).as_int();
        }
        else if constexpr (Traits::s_data_type == DataType::Boolean)
        {
            return etl::get<Word>(value).as_bool();
        }
        else if constexpr (Traits::s_data_type == DataType::Word || Traits::s_data_type == DataType::UnsignedInt)
        {
            return etl::get<Word>(value).as_uint();
        }
        else if constexpr (Traits::s_data_type == DataType::String)
        {
            return etl::get<Block>(value).as_string();
        }
        else
        {
            return etl::get<Block>(value);
        }
    }

    template <typename TBus>
    etl::expected<Value, Error> Host<TBus>::read(Command command)
    {
        const auto traits = get_runtime_traits(command);

        if (!traits.has_value())
        {
            return etl::unexpected(Error::UnknownCommand);
        }

        utils::ScopeGuard drain_bus(
            [this]()
            {
                while (m_bus.available())
                {
                    m_bus.read();
                }
            });

        m_bus.beginTransmission(m_battery_address);
        m_bus.write(static_cast<std::uint8_t>(command));

        if (auto result = m_bus.endTransmission(false); result != 0)
        {
            return etl::unexpected(error_from_bus_error(result));
        }

        const size_t expected_bytes = (traits.value().m_protocol == SmbusProtocol::Word) ? Word::s_wire_size : Block::s_max_wire_size;

        if (m_bus.requestFrom(m_battery_address, expected_bytes, true) == 0)
        {
            return etl::unexpected(Error::NoDataAvailable);
        }

        Value value;

        if (auto result = detail::consume_value(m_bus, value, traits.value()); !result.has_value())
        {
            return etl::unexpected(result.error());
        }
        return value;
    }

    template <typename TBus>
    template <Command Command>
    etl::expected<void, Error> Host<TBus>::write(typename CommandTraits<Command>::NativeType data)
    {
        using Traits = CommandTraits<Command>;
        Value value;

        if constexpr (Traits::s_protocol == SmbusProtocol::Word)
        {
            value = Word(data);
        }
        else
        {
            value = Block(data);
        }
        return write(Command, value);
    }

    template <typename TBus>
    etl::expected<void, Error> Host<TBus>::write(Command command, const Value &value)
    {
        const auto traits = get_runtime_traits(command);

        if (!traits.has_value())
        {
            return etl::unexpected(Error::UnknownCommand);
        }

        if (!detail::holds_expected_type(value, traits.value()))
        {
            return etl::unexpected(Error::UnexpectedValueType);
        }

        if (traits.value().m_access_type == AccessType::ReadOnly)
        {
            return etl::unexpected(Error::AccessDenied);
        }
        return detail::write_word(m_bus, command, value, traits.value(), m_battery_address);
    }
}
