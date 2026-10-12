#pragma once

#include <cstdint>

#include "sfg_sbs/block.hpp"

namespace sfg_sbs
{
    /// @brief SBS command codes, one row per entry of @ref commands.def - the Smart Battery Slave
    ///        Functions of the Smart Battery Data Specification Rev 1.1, Appendix A.
    enum class Command : std::uint8_t
    {
#define SFG_SBS_DEFINE_COMMAND(Name, Value, Type, Access, Default, Optional) Name = Value,
#include "sfg_sbs/commands.def"
#undef SFG_SBS_DEFINE_COMMAND
    };

#define SFG_SBS_DEFINE_COMMAND(Name, Value, Type, Access, Default, Optional) +1
    constexpr std::uint8_t s_command_count = 0
#include "sfg_sbs/commands.def"
        ;
#undef SFG_SBS_DEFINE_COMMAND
}
