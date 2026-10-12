/// @file main.cpp
/// @brief Defaults suite: the §4.4.1 initial values get_default_value prescribes. No bus is involved.
#include <cstdint>
#include <unity.h>

#include "test_fixture.hpp"

/// @brief RemainingCapacityAlarm() starts at 10% of DesignCapacity() (§4.4.1/§5.1.2).
/// @details The commmands.def table carries a sentinel for this command, because the value depends on battery state;
///          get_default_value computes it from the design capacity the caller supplies.
void test_remaining_capacity_alarm_default_is_ten_percent_of_design_capacity()
{
    TEST_ASSERT_EQUAL_UINT16(100, etl::get<sfg_sbs::Word>(sfg_sbs::get_default_value(sfg_sbs::Command::RemainingCapacityAlarm, 1000)).as_uint());
}

/// @brief Unity hook executed before each test case.
void setUp()
{
}

/// @brief Unity hook executed after each test case.
void tearDown()
{
}

/// @brief Test-program entry point for this suite.
/// @param argc Argument count, unused by the suite.
/// @param argv Argument vector, unused by the suite.
/// @return The number of failed test cases as reported by Unity. Zero means the whole suite passed.
int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_remaining_capacity_alarm_default_is_ten_percent_of_design_capacity);
    return UNITY_END();
}
