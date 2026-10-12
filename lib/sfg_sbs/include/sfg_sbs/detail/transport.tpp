#include "sfg_sbs/detail/transport.hpp"

#include <algorithm>
#include <etl/array.h>

namespace sfg_sbs::detail
{
    template <typename TBus>
    etl::expected<void, Error> write_word(TBus &bus, Command command, const Value &value, const CommandRuntimeTraits &traits, std::uint8_t target_address)
    {
        bus.beginTransmission(target_address);
        bus.write(static_cast<std::uint8_t>(command));

        if (auto result = emit_value(bus, value, traits); !result.has_value())
        {
            // The transfer is already open, so close it before reporting the type failure; the caller needs
            // that cause, so endTransmission's own code is discarded.
            bus.endTransmission();
            return etl::unexpected(result.error());
        }

        if (auto result = bus.endTransmission(); result != 0)
        {
            return etl::unexpected(error_from_bus_error(result));
        }
        return {};
    }

    template <typename TBus>
    etl::expected<void, Error> consume_value(TBus &bus, Value &value, const CommandRuntimeTraits &traits)
    {
        if (traits.m_protocol == SmbusProtocol::Block)
        {
            if (!bus.available())
            {
                return etl::unexpected(Error::NoDataAvailable);
            }

            // The first byte is the size of the block data.
            const std::uint8_t size = static_cast<std::uint8_t>(bus.read());
            etl::vector<std::uint8_t, Block::s_max_data_size> data;

            // If the size byte is larger than the maximum block size, we have a protocol violation.
            if (size > data.capacity())
            {
                return etl::unexpected(Error::BadSize);
            }

            for (std::uint8_t index = 0; index < size && bus.available(); index++)
            {
                data.push_back(static_cast<std::uint8_t>(bus.read()));
            }

            // If the bus has less data available than the bus indicated, we received too little data.
            if (data.size() < size)
            {
                return etl::unexpected(Error::TooLittleDataAvailable);
            }
            value = Block(data);
        }
        else
        {
            if (bus.available() < static_cast<int>(Word::s_wire_size))
            {
                return etl::unexpected(Error::TooLittleDataAvailable);
            }

            const std::uint8_t lsb = static_cast<std::uint8_t>(bus.read());
            const std::uint8_t msb = static_cast<std::uint8_t>(bus.read());
            const std::uint16_t raw_word = static_cast<std::uint16_t>((msb << 8) | lsb);
            value = Word(raw_word);
        }
        return {};
    }

    template <typename TBus>
    etl::expected<std::uint8_t, Error> emit_value(TBus &bus, const Value &value, const CommandRuntimeTraits &traits)
    {
        if (!holds_expected_type(value, traits))
        {
            return etl::unexpected(Error::UnexpectedValueType);
        }

        etl::vector<std::uint8_t, Value::s_max_wire_size> buffer;

        if (traits.m_protocol == SmbusProtocol::Block)
        {
            const auto &block = etl::get<Block>(value);
            const auto size = static_cast<std::uint8_t>(block.size());
            buffer.push_back(size);
            buffer.insert(buffer.end(), block.as_data(), block.as_data() + size);
        }
        else
        {
            const auto raw_word = etl::get<Word>(value).as_uint();
            buffer.push_back(static_cast<std::uint8_t>(raw_word & 0xFF));
            buffer.push_back(static_cast<std::uint8_t>((raw_word >> 8) & 0xFF));
        }
        return emit_raw(bus, buffer.data(), buffer.size());
    }

    template <typename TBus>
    etl::expected<std::uint8_t, Error> emit_raw(TBus &bus, const std::uint8_t *data, size_t size)
    {
        bus.write(data, size);
        return static_cast<std::uint8_t>(size);
    }

    template <typename TBus>
    etl::expected<std::uint8_t, Error> emit_raw(TBus &bus, std::uint8_t value, size_t count)
    {
        count = std::min(count, Value::s_max_wire_size);
        etl::array<std::uint8_t, Value::s_max_wire_size> buffer;
        buffer.fill(value);
        bus.write(buffer.data(), count);
        return static_cast<std::uint8_t>(count);
    }
}