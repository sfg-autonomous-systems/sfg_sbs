#pragma once

#include "sfg_sbs/data_type.hpp"

namespace sfg_sbs
{
    /// @brief SMBus transfer formats used by the SBS command set.
    ///
    /// @note Derived from the data-type vocabulary of Appendix B of the Smart Battery Data Specification
    ///       Rev 1.1: atomic units are framed as a Word transfer and the aggregate units as a Block transfer.
    ///       The per-command derivation lives in @ref CommandTraits::s_protocol.
    enum class SmbusProtocol
    {
        /// @brief 16-bit word transfer.
        Word,
        /// @brief Block transfer, i.e. count byte plus up to 32 data bytes.
        Block
    };
}