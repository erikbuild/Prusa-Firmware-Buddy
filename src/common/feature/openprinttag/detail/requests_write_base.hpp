/// @file
#pragma once

#include <string_view>

#include <feature/openprinttag/tool_tag.hpp>
#include <feature/openprinttag/detail/requests_base.hpp>

namespace buddy::openprinttag {

class WriteFieldRequestBase : public Request {

public:
    explicit WriteFieldRequestBase(ToolTagField tag_field)
        : Request(tag_field.section)
        , tag_field_(tag_field) {}

protected:
    const ToolTagField tag_field_;
};

template <typename Value_>
class WriteFieldRequestT : public WriteFieldRequestBase {

public:
    using Value = Value_;
    using Error = Request::Error;

    explicit WriteFieldRequestT(ToolTagField tag_field, Value value)
        : WriteFieldRequestBase(tag_field)
        , value_(value) {
    }

protected:
    Value value_;
};

class WriteFloatFieldRequest : public WriteFieldRequestT<float> {

public:
    // Inherit constructor
    using WriteFieldRequestT<float>::WriteFieldRequestT;
};

} // namespace buddy::openprinttag
