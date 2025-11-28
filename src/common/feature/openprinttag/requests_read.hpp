/// @file
#pragma once

#include <string_view>

#include "requests_base.hpp"
#include "tool_tag.hpp"

namespace buddy::openprinttag {

class ReadFieldRequest : public Request {

public:
    explicit ReadFieldRequest(ToolTagField tag_field)
        : Request(tag_field.section)
        , tag_field_(tag_field) {}

protected:
    const ToolTagField tag_field_;
};

template <typename T, typename Parent = ReadFieldRequest>
class ReadRequest : public Parent {

public:
    using Value = T;
    using Error = Request::Error;
    using Result = std::expected<Value, Error>;

public:
    /// Once @p finished, can be used to obtain the result
    /// Cannot be called before finished()
    Result result() const {
        assert(this->finished());

        if (this->has_error()) {
            return std::unexpected(this->error());
        }

        return result_;
    }

protected:
    // Inherit constructors
    using Parent::Parent;

protected:
    Value result_;
};

class EnumerateFieldsRequest final : public ReadRequest<std::span<const Field>, Request> {

public:
    /// @param buffer buffer for storing the text data
    explicit EnumerateFieldsRequest(ToolTag tag, Section section, std::span<Field> buffer)
        : ReadRequest(section_)
        , tag_(tag)
        , section_(section)
        , buffer_(buffer) {
    }

private:
    const ToolTag tag_;

    const Section section_;

    /// Buffer for storing the data.
    /// The actual result will then be a subspan of this buffer.
    std::span<Field> buffer_;
};

class ReadInt32Request : public ReadRequest<int32_t> {

public:
    // Inherit constructor
    using ReadRequest<int32_t>::ReadRequest;
};

template <typename Enum>
class ReadEnumRequest : public ReadRequest<Enum> {

public:
    // Inherit constructor
    using ReadRequest<Enum>::ReadRequest;
};

template <typename Enum>
class ReadEnumArrayRequest : public ReadRequest<std::span<const Enum>> {

public:
    // Inherit constructor
    using ReadRequest<std::span<const Enum>>::ReadRequest;
};

class ReadFloatRequest : public ReadRequest<float> {

public:
    // Inherit constructor
    using ReadRequest<float>::ReadRequest;
};

class ReadStringRequestBase : public ReadRequest<std::string_view> {

public:
    /// @param buffer buffer for storing the text data
    explicit ReadStringRequestBase(ToolTagField tag_field, std::span<char> buffer)
        : ReadRequest(tag_field)
        , buffer_(buffer) {
    }

protected:
    inline void set_buffer(std::span<char> set) {
        buffer_ = set;
    }

private:
    /// Buffer for storing the text data.
    /// The actual result will then be a subspan of this buffer.
    std::span<char> buffer_;
};

template <size_t array_size>
class ReadStringRequest : public ReadStringRequestBase {

public:
    explicit ReadStringRequest(ToolTagField tag_field)
        : ReadStringRequestBase(tag_field, std::span<char> {}) {
        set_buffer(array_);
    }

private:
    std::array<char, array_size> array_;
};

} // namespace buddy::openprinttag
