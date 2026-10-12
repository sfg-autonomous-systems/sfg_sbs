#include "test_utils.hpp"

std::string make_message(const char *subject, const char *expectation)
{
    return std::string(subject) + " " + expectation;
}
