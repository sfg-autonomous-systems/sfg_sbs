#pragma once

#include <optional>

#include "sfg_sbs/access_type.hpp"
#include "sfg_sbs/command.hpp"
#include "sfg_sbs/command_traits.hpp"
#include "sfg_sbs/data_type.hpp"
#include "sfg_sbs/smbus_protocol.hpp"

namespace sfg_sbs
{
    struct CommandRuntimeTraits
    {
    public:
        DataType m_data_type;
        AccessType m_access_type;
        SmbusProtocol m_protocol;
        size_t m_max_wire_size;
        bool m_optional;
    };

    constexpr std::optional<CommandRuntimeTraits> get_runtime_traits(Command command)
    {
        switch (command)
        {
#define SFG_SBS_DEFINE_COMMAND(Name, Value, Type, Access, Default, Optional) \
    case Command::Name:                                                      \
    {                                                                        \
        return CommandRuntimeTraits{                                         \
            CommandTraits<Command::Name>::s_data_type,                       \
            CommandTraits<Command::Name>::s_access_type,                     \
            CommandTraits<Command::Name>::s_protocol,                        \
            CommandTraits<Command::Name>::s_max_wire_size,                   \
            CommandTraits<Command::Name>::s_optional};                       \
    }
#include "sfg_sbs/commands.def"
#undef SFG_SBS_DEFINE_COMMAND

            default:
            {
                return std::nullopt;
            }
        }
    }
}
