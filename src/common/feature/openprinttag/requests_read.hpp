/// @file
#pragma once

#include <feature/openprinttag/detail/requests_meta.hpp>

namespace buddy::openprinttag {

template <CField auto field>
class ReadFieldRequest final : public FieldTypeTraits<FieldTraits<field>::field_type>::template ReadFieldRequest<field> {

public:
    using FieldTraits = openprinttag::FieldTraits<field>;
    using FieldTypeTraits = openprinttag::FieldTypeTraits<FieldTraits::field_type>;
    using BaseReadFieldRequest = FieldTypeTraits::template ReadFieldRequest<field>;

    template <typename... Args>
    explicit inline ReadFieldRequest(ToolTag tag, Args &&...args)
        : BaseReadFieldRequest(tag.field(field), std::forward<Args>(args)...) {
    }
};

} // namespace buddy::openprinttag
