#pragma once

#include <cstdint>
#include <etl/expected.h>

#include "sfg_sbs/command_runtime_traits.hpp"
#include "sfg_sbs/data_type.hpp"
#include "sfg_sbs/error.hpp"
#include "sfg_sbs/value.hpp"

namespace sfg_sbs::detail
{
    template <typename TBus>
    etl::expected<void, Error> write_word(TBus &bus, Command command, const Value &value, const CommandRuntimeTraits &traits, std::uint8_t target_address);

    template <typename TBus>
    etl::expected<void, Error> consume_value(TBus &bus, Value &value, const CommandRuntimeTraits &traits);

    template <typename TBus>
    etl::expected<std::uint8_t, Error> emit_value(TBus &bus, const Value &value, const CommandRuntimeTraits &traits);

    template <typename TBus>
    etl::expected<std::uint8_t, Error> emit_raw(TBus &bus, const std::uint8_t *data, size_t size);

    template <typename TBus>
    etl::expected<std::uint8_t, Error> emit_raw(TBus &bus, std::uint8_t value, size_t count);

    /// @brief Reports whether the variant holds the alternative this command's protocol requires.
    /// @param value The variant to check.
    /// @param traits The command's runtime traits.
    bool holds_expected_type(const Value &value, const CommandRuntimeTraits &traits);
}

#include "sfg_sbs/detail/transport.tpp"