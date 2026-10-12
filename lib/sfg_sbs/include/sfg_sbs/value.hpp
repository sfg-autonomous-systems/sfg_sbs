#pragma once

#include <algorithm>
#include <cstdint>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#include <etl/variant.h>
#pragma GCC diagnostic pop

#include "sfg_sbs/block.hpp"
#include "sfg_sbs/command.hpp"
#include "sfg_sbs/word.hpp"

namespace sfg_sbs
{
    /// @brief A command payload in wire form, corresponding to the two framings of Appendix B.
    class Value : public etl::variant<Word, Block>
    {
    public:
        /// @brief Max wire size in bytes of a value transfer.
        static constexpr size_t s_max_wire_size = std::max(Word::s_wire_size, Block::s_max_wire_size);

        using etl::variant<Word, Block>::variant;
    };

    /// @brief Retrieves the default value of the specified command as defined in @ref commands.def.
    /// @param command The command whose default value is requested.
    /// @param design_capacity This argument is mandatory for @ref Command::RemainingCapacityAlarm in order to
    ///                        compute the default value as specified by Battery Data Specification Rev 1.1, §5.1.2.
    /// @return The default value of the command.
    Value get_default_value(Command command, std::uint16_t design_capacity = 0);
}