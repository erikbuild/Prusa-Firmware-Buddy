/// @file
#pragma once

#include <utils/meta/value_pack.hpp>

#include <feature/openprinttag/detail/requests_multi_base.hpp>
#include <feature/openprinttag/requests_read.hpp>

namespace buddy::openprinttag {

/// Convenience class for handling many read field requests at once
/// Basically a tuple of multiple ReadFieldRequests
template <CField auto... fields_>
class MultiReadFieldRequestImpl final : public MultiRequestBase {

public:
    static constexpr auto fields = ValuePack<fields_...> {};
    static_assert(fields.size > 0);

public:
    MultiReadFieldRequestImpl(ToolTag tag)
        // Trick to force template unpacking to provide "tag" as a constructor argument for each request
        // The comma operator discards the `field` expression, but having it triggers the unpacking
        : requests_tuple { ((void)fields_, tag)... } {

        // Put pointers of all the requests in the tuple to the array
        [this]<size_t... index>(std::index_sequence<index...>) {
            ((requests_array_[index] = &std::get<index>(requests_tuple)), ...);
        }(std::make_index_sequence<fields.size>());

        // Set up the span inherited from MultiRequestBase
        requests_span_ = requests_array_;
    }

    /// @returns result of a ReadFieldRequest for the specified field
    template <CField auto field>
    inline ReadFieldRequest<field>::Result result() const {
        return request<field>().result();
    }

    /// @returns ReadFieldRequest for the specified field
    template <CField auto field>
    inline const ReadFieldRequest<field> &request() const {
        return std::get<ReadFieldRequest<field>>(requests_tuple);
    }

    /// @returns ReadFieldRequest for the specified field
    template <CField auto field>
    inline ReadFieldRequest<field> &request() {
        return std::get<ReadFieldRequest<field>>(requests_tuple);
    }

private:
    /// Tuple of all the requests the multirequest has (except for the sync request which is in the parent)
    std::tuple<ReadFieldRequest<fields_>...> requests_tuple;

    /// Array with pointers to all requests from the requests_tuple
    std::array<Request *, fields.size> requests_array_;
};

/// Same as @p MultiReadFieldRequest, but:
/// - Removes duplicates (that would otherwise throw errors)
/// - Unpacks nested ValuePacks
template <auto... values>
using MultiReadFieldRequest = decltype([] {
    constexpr auto all_unsorted = ValuePack<values...> {}.flatten().unique();

    // Group fields together by section to prevent unnecessary read cache misses on the reader
    constexpr auto main_f = [](CField auto field) { return field_section(field) == Section::main; };
    constexpr auto other_f = [](CField auto field) { return field_section(field) != Section::main; };

    constexpr auto main = all_unsorted.template filter<main_f>();
    constexpr auto other = all_unsorted.template filter<other_f>();

    return main.concatenate(other);
}())::template ApplyOn<MultiReadFieldRequestImpl>;

/// Basically a tuple of REFERENCES to specified fields ReadRequests.
/// Used for referencing a subset of requests from MultiReadFieldRequest for data operations
template <CField auto... fields_>
class MultiReadFieldRequestRef {

public:
    static constexpr auto fields = ValuePack<fields_...> {};

public:
    /// Constructs the ref objecct from a MultiReadFieldRequest (stores a subset of references)
    template <CField auto... src_fields>
    MultiReadFieldRequestRef(const MultiReadFieldRequestImpl<src_fields...> &req)
        : refs { req.template request<fields_>()... } {}

    /// Constructs the ref objecct from a potentially bigger MultiReadFieldRequestRef
    template <CField auto... src_fields>
    MultiReadFieldRequestRef(const MultiReadFieldRequestRef<src_fields...> &req)
        : refs { req.template request<fields_>()... } {}

public:
    template <CField auto field>
    using Item = std::reference_wrapper<const ReadFieldRequest<field>>;

    template <CField auto field>
    inline const ReadFieldRequest<field> &request() const {
        return std::get<Item<field>>(refs);
    }

    template <CField auto field>
    inline ReadFieldRequest<field>::Result result() const {
        return request<field>().result();
    }

    /// @returns whether all results are either present or have the field_missing error
    /// All other errors are considered invalid and should not be used for any data operations
    [[nodiscard]] bool are_results_valid() const {
        const auto is_result_valid = [this]<auto field>() {
            const auto r = result<field>();
            return r.has_value() || r == std::unexpected { Request::Error::field_not_present };
        };
        return (is_result_valid.template operator()<fields_>() && ...);
    }

public:
    std::tuple<Item<fields_>...> refs;
};

}; // namespace buddy::openprinttag
