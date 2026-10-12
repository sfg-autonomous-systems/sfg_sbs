#pragma once

namespace sfg_sbs
{
    /// @brief Access types as listed in Smart Battery Data Specification Rev 1.1, Appendix A.
    ///
    /// @note Models the Smart Battery Slave Functions of Appendix A only.
    ///       The specification's Smart Battery Master Functions are write-only
    ///       and are not described by this enum.
    enum class AccessType
    {
        ReadOnly,
        ReadWrite,
    };
}