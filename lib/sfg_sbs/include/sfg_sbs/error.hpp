#pragma once

#include <cstdint>

namespace sfg_sbs
{
    enum class Error
    {
        /// @brief Maps to I2C Data Too Long error returned by I2C.endTransmission().
        BusDataTooLong,

        /// @brief Maps to I2C Address NACK error returned by I2C.endTransmission().
        BusAddressNack,

        /// @brief Maps to I2C Data NACK error returned by I2C.endTransmission().
        BusDataNack,

        /// @brief Maps to any other I2C error returned by I2C.endTransmission().
        BusOtherError,

        /// @brief Returned when the type of the variant value held by Value does not match
        ///        the command's expected data type.
        UnexpectedValueType,

        /// @brief Returned when a command is being processed for which no traits are defined.
        UnknownCommand,

        /// @brief Returned when a read operation is attempted but no data is available on the bus.
        NoDataAvailable,

        /// @brief Returned whenever the bus has less data available than expected.
        ///        For example, a Read Word command expects 2 bytes. A Read Block
        ///        command expects at least 1 byte (the size byte) followed by
        ///        the number of bytes indicated by the size byte.
        TooLittleDataAvailable,

        /// @brief Returned when Battery::handle_request() is called before
        ///        Battery::handle_receive() has been called to set the active command.
        NoActiveCommand,

        /// @brief Returned when a battery handles a request for data but no read handler is bound to handle it.
        NoReadHandlerBound,

        /// @brief Returned when a battery handles a Write Block operation by the host but no write handler is bound to handle it.
        NoWriteHandlerBound,

        /// @brief Returned when the count byte of a Write Block, or the size byte of a Read Block,
        ///        exceeds the Appendix B block capacity. Mirrors the Appendix C BadSize code, the
        ///        code the battery records for a write with an incorrect data-block size.
        ///
        /// @note The library reports the same value from the host read path, where the specification
        ///        defines no code.
        BadSize,

        /// @brief Returned when a Write Block or Write Word operation is attempted on a read-only command
        ///        as specified in Battery Data Specification Rev 1.1, Appendix C.
        AccessDenied
    };

    Error error_from_bus_error(int bus_error);
}