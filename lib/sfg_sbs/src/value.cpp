#include "sfg_sbs/value.hpp"

#include <etl/string_view.h>

#include "sfg_sbs/command_traits.hpp"
#include "sfg_sbs/smbus_protocol.hpp"

namespace sfg_sbs
{
    constexpr bool default_value_fits(std::uint64_t raw)
    {
        return raw <= 0xFFFF;
    }

    constexpr bool default_value_fits(etl::string_view raw)
    {
        return raw.size() <= Block::s_max_data_size;
    }

    template <Command Command>
    Word make_default_value(std::uint16_t raw)
    {
        static_assert(CommandTraits<Command>::s_protocol == SmbusProtocol::Word, "Command default value disagrees with its data type.");
        return Word{raw};
    }

    template <Command Command>
    Block make_default_value(etl::string_view raw)
    {
        static_assert(CommandTraits<Command>::s_protocol == SmbusProtocol::Block, "Command default value disagrees with its data type.");
        return Block{raw};
    }

    Value get_default_value(Command command, std::uint16_t design_capacity)
    {
        if (command == Command::RemainingCapacityAlarm)
        {
            // Smart Battery Data Specification Rev 1.1, §5.1.2 states that the
            // default value of remaining capacity alarm is 10% of the design capacity.
            //
            // Since this cannot be encoded in commands.def, we handle it here.
            return Word{static_cast<std::uint16_t>((design_capacity + 5) / 10)};
        }

        switch (command)
        {
#define SFG_SBS_DEFINE_COMMAND(Name, Value, Type, Access, Default, Optional)                                       \
    case Command::Name:                                                                                            \
    {                                                                                                              \
        static_assert(default_value_fits(Default), "Command default value exceeds its data type's maximum size."); \
        return make_default_value<Command::Name>(Default);                                                         \
    }
#include "sfg_sbs/commands.def"
#undef SFG_SBS_DEFINE_COMMAND

            default:
            {
                return Word{};
            }
        }
    }
}