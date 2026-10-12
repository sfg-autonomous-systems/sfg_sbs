#include <string>

/// @brief Joins a table row's subject and an assertion's expectation into one message sentence, e.g.
///        "A truncated block write" and "should be drained." into "A truncated block write should be drained.".
/// @param subject The table row's subject, starting with a capital letter.
/// @param expectation What the assertion expects of the subject, ending with a full stop.
/// @return The message sentence.
std::string make_message(const char *subject, const char *expectation);