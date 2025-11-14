#include "requests_base.hpp"

#include <logging/log.hpp>
#include <feature/openprinttag/request_manager.hpp>

LOG_COMPONENT_DEF(OpenPrintTag, logging::Severity::info);

namespace buddy::openprinttag {

// Do mind sizeof(Request) greatly.
// The class will get allocated on the stack many times at the same time, so every byte counts.
static_assert(sizeof(Request) == 12);

Request::~Request() {
    manager().remove_request({}, *this);
}

void Request::issue() {
    manager().add_request({}, *this);
}

void Request::set_finished(std::expected<std::monostate, Error> result) {
    assert(!finished_);
    finished_ = true;
    error_ = result.error_or(Error::_cnt);
}

} // namespace buddy::openprinttag
