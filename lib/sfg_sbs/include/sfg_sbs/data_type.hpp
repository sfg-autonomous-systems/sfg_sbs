#pragma once

namespace sfg_sbs
{
    /// @brief Data types as defined by Smart Battery Data Specification Rev 1.1, Appendix B.
    ///        "Units describing atomic data types" and "Units describing aggregate/packed data types".
    ///
    /// @note SBData 1.1 has no command that uses Char or Byte: the command set defines no byte transfer functions.
    ///       Therefore these types are omitted.
    enum class DataType
    {
        // Units describing atomic data types.
        Int,
        UnsignedInt,
        Word,
        Boolean,

        // Units describing aggregate/packed data types.
        Data,
        String,
    };
}