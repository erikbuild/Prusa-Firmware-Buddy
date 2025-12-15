/// @file
#pragma once

#include <feature/openprinttag/tool_tag.hpp>
#include <feature/openprinttag/requests_read_multi.hpp>

namespace buddy::openprinttag {

/// GUI and functionality niceties for performing OPT MultiRequests
/// - Automatically shows an error if tag is nullopt
/// - Automatic retries on failure
/// - Possible recovery sequences (TODO dynamic region reformatting and similar)
///
/// Upon success, the result is stored in result (the MultiRequest may get issued multiple times because of retries)
/// @returns if the request was successful
/// !!! To be executed on display thread only
[[nodiscard]] bool multirequest_with_troubleshooting(buddy::openprinttag::MultiRequestBase &multirequest);

} // namespace buddy::openprinttag
