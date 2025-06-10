#include "nfc_task.hpp"

#include <mutex>

#include <freertos/timing.hpp>

#include <cyphal_anfc_node.hpp>

#include <prusa3d/nfc/event/Event_1_0.h>
#include <prusa3d/nfc/util/ReaderError_1_0.h>

namespace {
bool is_valid_section(std::underlying_type_t<NFCSection> section) {
    return section < std::to_underlying(NFCSection::_cnt);
}

std::optional<PrusaNFCReader::NFCTagField> parse_request_field(const prusa3d_nfc_util_TagField_1_0 &field) {
    if (!is_valid_section(field.section.value)) {
        return std::nullopt;
    }

    return PrusaNFCReader::NFCTagField {
        .tag = field.tag.value,
        .section = static_cast<NFCSection>(field.section.value),
        .field = field.field.value,
    };
}
} // namespace

bool NFCTask::enqueue_serialized_request(const std::span<const uint8_t> &data) {
    using ReqTraits = prusa3d_nfc_command_Request_Request_1_0_Traits;
    using Size = uint16_t;

    void *data_copy;
    {
        std::lock_guard lg(job_queue_mutex_);
        data_copy = nfc_task.job_queue_data_heap_.alloc(data.size() + sizeof(Size));
    }

    // Failed to serialize -> the queue is full
    if (!data_copy) {
        return false;
    }

    // Store size in the allocated region as well, inplace_function wouldn't fit another variable in captures
    *static_cast<Size *>(data_copy) = data.size();
    memcpy(static_cast<std::byte *>(data_copy) + sizeof(Size), data.data(), data.size());

    const auto job = [this, data_copy]() {
        ReqTraits::Type request;
        size_t size = *static_cast<const Size *>(data_copy);

        // Deserialize the message only for processing. Deserialized messages takes much more space than a serialized one
        const auto deserialize_result = ReqTraits::deserialize(&request, static_cast<const uint8_t *>(data_copy) + sizeof(Size), &size);

        // After deserializing, we no longer need the data
        {
            std::lock_guard lock { job_queue_mutex_ };
            job_queue_data_heap_.free(data_copy);
        }

        // Failed to deserialize the request -> drop
        if (deserialize_result < 0) {
            return;
        }

        prusa3d_nfc_event_Event_1_0 response;
        prusa3d_nfc_event_EventData_1_0_select_request_done_(&response.data);
        response.data.request_done.request_id = request.request_id;

        auto &rreq = request.request;
        auto &result = response.data.request_done.result;

        if (prusa3d_nfc_request_RequestData_1_0_is_read_field_(&rreq)) {
            prusa3d_nfc_request_RequestResult_1_0_select_read_field_(&result);
            handle_read_field_request(rreq.read_field, result.read_field);

        } else if (prusa3d_nfc_request_RequestData_1_0_is_write_field_(&rreq)) {
            prusa3d_nfc_request_RequestResult_1_0_select_write_field_(&result);
            handle_write_field_request(rreq.write_field, result.write_field);

        } else if (prusa3d_nfc_request_RequestData_1_0_is_reset_state_(&rreq)) {
            reader_.reset_state();

        } else if (prusa3d_nfc_request_RequestData_1_0_is_forget_tag_(&rreq)) {
            reader_.forget_tag(rreq.forget_tag.tag.value);

        } else if (prusa3d_nfc_request_RequestData_1_0_is_set_params_(&rreq)) {
            auto &req = rreq.set_params;

            const auto mime_len = req.mime_type.value.count;
            if (mime_len > mime_type_buffer_.size()) {
                return;
            }
            memcpy(mime_type_buffer_.data(), req.mime_type.value.elements, mime_len);

            reader_.set_params(PrusaNFCReader::Params {
                .mime_type = std::string_view(mime_type_buffer_.data(), mime_len),
                .has_meta_region = req.has_meta_region,
            });

        } else if (prusa3d_nfc_request_RequestData_1_0_is_enumerate_fields_(&rreq)) {
            prusa3d_nfc_request_RequestResult_1_0_select_enumerate_fields_(&result);
            handle_enumerate_fields_request(rreq.enumerate_fields, result.enumerate_fields);
        }

        can_node.enqueue_event(response);
    };

    if (!enqueue_job(job)) {
        std::lock_guard lock { job_queue_mutex_ };
        job_queue_data_heap_.free(data_copy);
        return false;
    }

    return true;
}

void NFCTask::task() {
    nfc::readers_init();
    while (true) {
        // Process a reader event
        if (PrusaNFCReader::Event e; reader_.get_event(e)) {
            handle_event(e);
        }

        // Process a request
        // Dequeues are done from a single thread, so we don't need a mutex
        if (!job_queue_.isEmpty()) {
            job_queue_.dequeue()();
        }

        freertos::delay(1);
    }
}

bool NFCTask::enqueue_job(Job &&job) {
    // Enqueing might be attempted simultaneously from multiple threads, so we need to protect it with a mutex
    std::unique_lock lock(job_queue_mutex_);
    return job_queue_.enqueue(std::move(job));
}

void NFCTask::handle_event(const PrusaNFCReader::Event &event) {
    prusa3d_nfc_event_Event_1_0 msg;

    std::visit([&]<typename T>(const T &e) {
        if constexpr (std::is_same_v<T, INFCReader::TagDetectedEvent>) {
            prusa3d_nfc_event_EventData_1_0_select_tag_detected_(&msg.data);
            msg.data.tag_detected = {
                .tag { e.tag },
                .antenna { e.antenna },
            };

        } else if constexpr (std::is_same_v<T, INFCReader::TagLostEvent>) {
            prusa3d_nfc_event_EventData_1_0_select_tag_lost_(&msg.data);
            msg.data.tag_lost.tag.value = e.tag;

        } else {
            static_assert(false, "Unhandled event type");
        }
    },
        event);

    can_node.enqueue_event(msg);
}

void NFCTask::handle_read_field_request(const prusa3d_nfc_request_ReadField_1_0 &request, prusa3d_nfc_util_ValueOrError_1_0 &result) {
    using Error = PrusaNFCReader::Error;

    const auto set_error_result = [&](Error error) {
        prusa3d_nfc_util_ValueOrError_1_0_select_error_(&result);
        result._error._error = std::to_underlying(error);
    };

    const auto field_opt = parse_request_field(request.field);
    if (!field_opt) {
        set_error_result(Error::other);
        return;
    }
    const auto field = *field_opt;

    prusa3d_nfc_util_ValueOrError_1_0_select_value_(&result);
    auto &value = result.value;

    static_assert(prusa3d_nfc_util_Value_1_0_UNION_OPTION_COUNT_ == 7);
    switch (request.value_type.value) {

    case prusa3d_nfc_util_ValueType_1_0_BOOL_: {
        if (auto r = reader_.read_field_bool(field)) {
            prusa3d_nfc_util_Value_1_0_select_bool__(&value);
            value.bool_ = *r;

        } else {
            set_error_result(r.error());
        }
        break;
    }

    case prusa3d_nfc_util_ValueType_1_0_INT32_: {
        if (auto r = reader_.read_field_int32(field)) {
            prusa3d_nfc_util_Value_1_0_select_int32__(&value);
            value.int32_ = *r;

        } else {
            set_error_result(r.error());
        }
        break;
    }

    case prusa3d_nfc_util_ValueType_1_0_INT64_: {
        if (auto r = reader_.read_field_int64(field)) {
            prusa3d_nfc_util_Value_1_0_select_int64__(&value);
            value.int64_ = *r;

        } else {
            set_error_result(r.error());
        }
        break;
    }

    case prusa3d_nfc_util_ValueType_1_0_FLOAT_: {
        if (auto r = reader_.read_field_float(field)) {
            prusa3d_nfc_util_Value_1_0_select_float__(&value);
            value.float_ = *r;

        } else {
            set_error_result(r.error());
        }
        break;
    }

    case prusa3d_nfc_util_ValueType_1_0_STRING: {
        if (auto r = reader_.read_field_string(field, reinterpret_cast<char(&)[uavcan_primitive_String_1_0_value_ARRAY_CAPACITY_]>(value._string.value.elements))) {
            prusa3d_nfc_util_Value_1_0_select_string_(&value);
            value._string.value.count = r->size();

        } else {
            set_error_result(r.error());
        }
        break;
    }

    case prusa3d_nfc_util_ValueType_1_0_BYTES: {
        if (auto r = reader_.read_field_bytes(field, reinterpret_cast<std::byte(&)[uavcan_primitive_array_Natural8_1_0_value_ARRAY_CAPACITY_]>(value.bytes.value.elements))) {
            prusa3d_nfc_util_Value_1_0_select_bytes_(&value);
            value.bytes.value.count = r->size();

        } else {
            set_error_result(r.error());
        }
        break;
    }

    case prusa3d_nfc_util_ValueType_1_0_UINT16_ARRAY: {
        if (auto r = reader_.read_field_uint16_array(field, value.uint16_array.value.elements)) {
            prusa3d_nfc_util_Value_1_0_select_uint16_array_(&value);
            value.bytes.value.count = r->size();

        } else {
            set_error_result(r.error());
        }
        break;
    }

    default:
        set_error_result(Error::other);
        return;
    }
}

void NFCTask::handle_write_field_request(const prusa3d_nfc_request_WriteField_1_0 &request, prusa3d_nfc_util_ReaderError_1_0 &result) {
    auto &error = result._error;

    const auto field_opt = parse_request_field(request.field);
    if (!field_opt) {
        error = prusa3d_nfc_util_ReaderError_1_0_OTHER;
        return;
    }
    const auto field = *field_opt;

    PrusaNFCReader::IOResult<PrusaNFCReader::WriteReport> io_result;

    static_assert(prusa3d_nfc_util_Value_1_0_UNION_OPTION_COUNT_ == 7);
    if (prusa3d_nfc_util_Value_1_0_is_bool__(&request.value)) {
        io_result = reader_.write_field_bool(field, request.value.bool_);

    } else if (prusa3d_nfc_util_Value_1_0_is_int32__(&request.value)) {
        io_result = reader_.write_field_int32(field, request.value.int32_);

    } else if (prusa3d_nfc_util_Value_1_0_is_int64__(&request.value)) {
        io_result = reader_.write_field_int64(field, request.value.int64_);

    } else if (prusa3d_nfc_util_Value_1_0_is_float__(&request.value)) {
        io_result = reader_.write_field_float(field, request.value.float_);

    } else if (prusa3d_nfc_util_Value_1_0_is_string_(&request.value)) {
        const auto &val = request.value._string.value;
        io_result = reader_.write_field_string(field, std::string_view(reinterpret_cast<const char *>(val.elements), val.count));

    } else if (prusa3d_nfc_util_Value_1_0_is_bytes_(&request.value)) {
        const auto &val = request.value.bytes.value;
        io_result = reader_.write_field_bytes(field, std::basic_string_view<std::byte>(reinterpret_cast<const std::byte *>(val.elements), val.count));

    } else if (prusa3d_nfc_util_Value_1_0_is_uint16_array_(&request.value)) {
        const auto &val = request.value.uint16_array.value;
        io_result = reader_.write_field_uint16_array(field, std::span<const uint16_t>(val.elements, val.count));

    } else {
        io_result = std::unexpected(PrusaNFCReader::Error::other);
    }

    if (io_result.has_value()) {
        error = prusa3d_nfc_util_ReaderError_1_0_NO_ERROR;
    } else {
        error = std::to_underlying(io_result.error());
    }
}

void NFCTask::handle_enumerate_fields_request(const prusa3d_nfc_request_EnumerateFields_1_0 &request, prusa3d_nfc_request_EnumerateFieldsResult_1_0 &result) {
    if (!is_valid_section(request.section.value)) {
        prusa3d_nfc_request_EnumerateFieldsResult_1_0_select_error_(&result);
        result._error._error = prusa3d_nfc_util_ReaderError_1_0_OTHER;
        return;
    }

    const auto io_result = reader_.enumerate_fields(static_cast<NFCTagID>(request.tag.value), static_cast<NFCSection>(request.section.value), result.fields.value.elements);
    if (!io_result) {
        prusa3d_nfc_request_EnumerateFieldsResult_1_0_select_error_(&result);
        result._error._error = std::to_underlying(io_result.error());
        return;
    }

    prusa3d_nfc_request_EnumerateFieldsResult_1_0_select_fields_(&result);
    result.fields.value.count = io_result->size();
}
