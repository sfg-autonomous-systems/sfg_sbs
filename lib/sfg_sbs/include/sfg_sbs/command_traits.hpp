#pragma once

#include <cstdint>

#include "sfg_sbs/access_type.hpp"
#include "sfg_sbs/block.hpp"
#include "sfg_sbs/command.hpp"
#include "sfg_sbs/data_type.hpp"
#include "sfg_sbs/data_type_to_native_type_mapper.hpp"
#include "sfg_sbs/smbus_protocol.hpp"
#include "sfg_sbs/word.hpp"

namespace sfg_sbs
{
    /// @brief Compile-time traits of a single command, derived from its row in @ref commands.def.
    ///
    /// The command rows come from the Smart Battery Data Specification Rev 1.1, Appendix A.
    /// The Access column yields @ref s_access_type and the Data column yields @ref s_data_type.
    ///
    /// @ref NativeType and @ref s_protocol are derived from the data type, defined by Appendix B:
    /// @ref NativeType via @ref DataTypeToNativeTypeMapper, and @ref s_protocol by the
    /// framing rule stated there.
    template <Command Command>
    struct CommandTraits;

#define SFG_SBS_DEFINE_COMMAND(Name, Value, Type, Access, Default, Optional)                                                        \
    template <>                                                                                                                     \
    struct CommandTraits<Command::Name>                                                                                             \
    {                                                                                                                               \
    public:                                                                                                                         \
        using NativeType = DataTypeToNativeTypeMapper<DataType::Type>::NativeType;                                                  \
                                                                                                                                    \
        static constexpr DataType s_data_type = DataType::Type;                                                                     \
        static constexpr AccessType s_access_type = AccessType::Access;                                                             \
        static constexpr SmbusProtocol s_protocol = []()                                                                            \
        {                                                                                                                           \
            switch (DataType::Type)                                                                                                 \
            {                                                                                                                       \
                case DataType::Int:                                                                                                 \
                case DataType::UnsignedInt:                                                                                         \
                case DataType::Word:                                                                                                \
                case DataType::Boolean:                                                                                             \
                {                                                                                                                   \
                    return SmbusProtocol::Word;                                                                                     \
                }                                                                                                                   \
                case DataType::String:                                                                                              \
                case DataType::Data:                                                                                                \
                {                                                                                                                   \
                    return SmbusProtocol::Block;                                                                                    \
                }                                                                                                                   \
            }                                                                                                                       \
        }();                                                                                                                        \
        static constexpr size_t s_max_wire_size = (s_protocol == SmbusProtocol::Word ? Word::s_wire_size : Block::s_max_wire_size); \
        static constexpr bool s_optional = Optional;                                                                                \
    };
#include "sfg_sbs/commands.def"
#undef SFG_SBS_DEFINE_COMMAND
}
