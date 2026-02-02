/// @file
#pragma once

#include <feature/openprinttag/detail/requests_meta.hpp>
#include <feature/openprinttag/tool_tag.hpp>
#include <requests_data_stub.hpp>

namespace buddy::openprinttag {

template <CField auto field>
class WriteFieldRequest final : public FieldTypeTraits<FieldTraits<field>::field_type>::template WriteFieldRequest<field> {

public:
    using FieldTraits = openprinttag::FieldTraits<field>;
    using FieldTypeTraits = openprinttag::FieldTypeTraits<FieldTraits::field_type>;
    using BaseWriteFieldRequest = FieldTypeTraits::template WriteFieldRequest<field>;
    using Value = BaseWriteFieldRequest::Value;

    explicit inline WriteFieldRequest(ToolTag tag, const Value &value)
        : BaseWriteFieldRequest(tag.field(field), value) {

        stub_data[tag.field(field)] = value;
        write_count++;
        this->set_finished(std::monostate {});
    }
};

} // namespace buddy::openprinttag
