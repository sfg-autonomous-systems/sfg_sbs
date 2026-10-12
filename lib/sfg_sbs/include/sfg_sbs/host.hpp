#pragma once

#include <cstdint>
#include <etl/expected.h>

#include "sfg_sbs/command_traits.hpp"
#include "sfg_sbs/error.hpp"
#include "sfg_sbs/value.hpp"

namespace sfg_sbs
{
    template <typename TBus>
    class Host
    {
    public:
        explicit Host(TBus &bus, std::uint8_t battery_address = 0x0B);

        template <Command Command>
        etl::expected<typename CommandTraits<Command>::NativeType, Error> read();
        etl::expected<Value, Error> read(Command command);

        template <Command Command>
        etl::expected<void, Error> write(typename CommandTraits<Command>::NativeType data);
        etl::expected<void, Error> write(Command command, const Value &value);

    private:
        TBus &m_bus;
        std::uint8_t m_battery_address;
    };
}

#include "sfg_sbs/host.tpp"