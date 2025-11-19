#include "nfc_task.hpp"

#include <mutex>

#include <freertos/timing.hpp>

#include <prusa3d/nfc/event/Event_1_0.h>
#include <prusa3d/nfc/util/ReaderError_1_0.h>

using namespace openprinttag;

namespace {
bool is_valid_section(std::underlying_type_t<Section> section) {
    return section < std::to_underlying(Section::_cnt);
}

std::optional<OPTReader::TagField> parse_request_field(const prusa3d_nfc_util_TagField_1_0 &field) {
    if (!is_valid_section(field.section.value)) {
        return std::nullopt;
    }

    return OPTReader::TagField {
        .tag = field.tag.value,
        .section = static_cast<Section>(field.section.value),
        .field = field.field.value,
    };
}

template <typename T>
uint8_t io_result_to_error(const OPTReader::IOResult<T> &io_result) {
    if (io_result.has_value()) {
        return prusa3d_nfc_util_ReaderError_1_0_NO_ERROR;
    } else {
        return std::to_underlying(io_result.error());
    }
}

template <typename T>
uint8_t io_result_to_error(const OPTBackend::IOResult<T> &io_result) {
    if (io_result.has_value()) {
        return prusa3d_nfc_util_ReaderError_1_0_NO_ERROR;
    } else {
        return std::to_underlying(OPTReader::to_reader_error(io_result.error()));
    }
}
} // namespace

NFCTask::NFCTask(OPTBackend &backend, EventCallback &&event_callback, HWReconfigurationCallback &&hw_reconfiguration_callback)
    : event_callback_ { std::move(event_callback) }
    , hw_reconfiguration_callback_ { std::move(hw_reconfiguration_callback) }
    , reader_ { backend } //
{}

bool NFCTask::enqueue_serialized_request(const std::span<const uint8_t> &data) {
    using ReqTraits = prusa3d_nfc_command_Request_Request_1_0_Traits;
    using Size = uint16_t;

    void *data_copy;
    {
        std::lock_guard lg(job_queue_mutex_);
        data_copy = job_queue_data_heap_.alloc(data.size() + sizeof(Size));
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
        prusa3d_nfc_request_RequestResult_1_0_select_empty_(&response.data.request_done.result);
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

            reader_.set_params(OPTReader::Params {
                .mime_type = std::string_view(mime_type_buffer_.data(), mime_len),
                .has_meta_region = req.has_meta_region,
            });

        } else if (prusa3d_nfc_request_RequestData_1_0_is_enumerate_fields_(&rreq)) {
            prusa3d_nfc_request_RequestResult_1_0_select_enumerate_fields_(&result);
            handle_enumerate_fields_request(rreq.enumerate_fields, result.enumerate_fields);

        } else if (prusa3d_nfc_request_RequestData_1_0_is_raw_read_(&rreq)) {
            prusa3d_nfc_request_RequestResult_1_0_select_raw_read_(&result);
            handle_raw_read_request(rreq.raw_read, result.raw_read);

        } else if (prusa3d_nfc_request_RequestData_1_0_is_raw_write_(&rreq)) {
            prusa3d_nfc_request_RequestResult_1_0_select_raw_write_(&result);
            handle_raw_write_request(rreq.raw_write, result.raw_write);

        } else if (prusa3d_nfc_request_RequestData_1_0_is_initialize_tag_(&rreq)) {
            prusa3d_nfc_request_RequestResult_1_0_select_initialize_tag_(&result);
            handle_initialize_tag_request(rreq.initialize_tag, result.initialize_tag);

        } else if (prusa3d_nfc_request_RequestData_1_0_is_unlock_tag_(&rreq)) {
            prusa3d_nfc_request_RequestResult_1_0_select_unlock_tag_(&result);
            handle_unlock_tag_request(rreq.unlock_tag, result.unlock_tag);

        } else if (prusa3d_nfc_request_RequestData_1_0_is_set_debug_config_(&rreq)) {
            prusa3d_nfc_request_RequestResult_1_0_select_empty_(&result);
            handle_set_debug_config_request(rreq.set_debug_config);

        } else if (prusa3d_nfc_request_RequestData_1_0_is_enable_radio_(&rreq)) {
            radio_enabled_ = true;

        } else if (prusa3d_nfc_request_RequestData_1_0_is_disable_radio_(&rreq)) {
            radio_enabled_ = false;
        }

        else if (prusa3d_nfc_request_RequestData_1_0_is_get_tag_uid_(&rreq)) {
            prusa3d_nfc_request_RequestResult_1_0_select_get_tag_uid_(&result);
            handle_get_tag_uid_request(rreq.get_tag_uid, result.get_tag_uid);
        }

        event_callback_(response);
    };

    if (!enqueue_job(job)) {
        std::lock_guard lock { job_queue_mutex_ };
        job_queue_data_heap_.free(data_copy);
        return false;
    }

    return true;
}

void NFCTask::task() {
    while (true) {
        // Process a reader event
        if (OPTReader::Event e; radio_enabled_ && reader_.get_event(e, freertos::millis())) {
            handle_event(e);

            // We've done something -> skip the delay
            continue;
        }

        // Process a request
        // Dequeues are done from a single thread, so we don't need a mutex
        if (!job_queue_.isEmpty()) {
            job_queue_.dequeue()();

            // We've done something -> skip the delay
            continue;
        }

        // When radio is disabled, there's not much stuff to do, so let's be a bit lazy
        freertos::delay(radio_enabled_ ? 1 : 10);
    }
}

bool NFCTask::enqueue_job(Job &&job) {
    // Enqueing might be attempted simultaneously from multiple threads, so we need to protect it with a mutex
    std::unique_lock lock(job_queue_mutex_);
    return job_queue_.enqueue(std::move(job));
}

void NFCTask::handle_event(const OPTReader::Event &event) {
    prusa3d_nfc_event_Event_1_0 msg;

    std::visit([&]<typename T>(const T &e) {
        if constexpr (std::is_same_v<T, OPTBackend::TagDetectedEvent>) {
            prusa3d_nfc_event_EventData_1_0_select_tag_detected_(&msg.data);
            msg.data.tag_detected = {
                .tag { e.tag },
                .antenna { e.antenna },
            };

        } else if constexpr (std::is_same_v<T, OPTBackend::TagLostEvent>) {
            prusa3d_nfc_event_EventData_1_0_select_tag_lost_(&msg.data);
            msg.data.tag_lost.tag.value = e.tag;

        } else {
            static_assert(false, "Unhandled event type");
        }
    },
        event);

    event_callback_(msg);
}

void NFCTask::handle_read_field_request(const prusa3d_nfc_request_ReadField_1_0 &request, prusa3d_nfc_util_ValueOrError_1_0 &result) {
    if (!radio_enabled_) {
        prusa3d_nfc_util_ValueOrError_1_0_select_error_(&result);
        result._error._error = prusa3d_nfc_util_ReaderError_1_0_RADIO_DISABLED;
        return;
    }

    using Error = OPTReader::Error;

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
        prusa3d_nfc_util_Value_1_0_select_string_(&value);
        if (auto r = reader_.read_field_string(field, reinterpret_cast<char(&)[uavcan_primitive_String_1_0_value_ARRAY_CAPACITY_]>(value._string.value.elements))) {
            value._string.value.count = r->size();

        } else {
            set_error_result(r.error());
        }
        break;
    }

    case prusa3d_nfc_util_ValueType_1_0_BYTES: {
        prusa3d_nfc_util_Value_1_0_select_bytes_(&value);
        if (auto r = reader_.read_field_bytes(field, reinterpret_cast<std::byte(&)[uavcan_primitive_array_Natural8_1_0_value_ARRAY_CAPACITY_]>(value.bytes.value.elements))) {
            value.bytes.value.count = r->size();

        } else {
            set_error_result(r.error());
        }
        break;
    }

    case prusa3d_nfc_util_ValueType_1_0_UINT16_ARRAY: {
        prusa3d_nfc_util_Value_1_0_select_uint16_array_(&value);
        if (auto r = reader_.read_field_uint16_array(field, value.uint16_array.value.elements)) {
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
    if (!radio_enabled_) {
        error = prusa3d_nfc_util_ReaderError_1_0_RADIO_DISABLED;
        return;
    }

    const auto field_opt = parse_request_field(request.field);
    if (!field_opt) {
        error = prusa3d_nfc_util_ReaderError_1_0_OTHER;
        return;
    }
    const auto field = *field_opt;

    OPTReader::IOResult<OPTReader::WriteReport> io_result;

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
        io_result = std::unexpected(OPTReader::Error::other);
    }

    error = io_result_to_error(io_result);
}

void NFCTask::handle_enumerate_fields_request(const prusa3d_nfc_request_EnumerateFields_1_0 &request, prusa3d_nfc_request_EnumerateFieldsResult_1_0 &result) {
    if (!radio_enabled_) {
        prusa3d_nfc_request_EnumerateFieldsResult_1_0_select_error_(&result);
        result._error._error = prusa3d_nfc_util_ReaderError_1_0_RADIO_DISABLED;
        return;
    }

    if (!is_valid_section(request.section.value)) {
        prusa3d_nfc_request_EnumerateFieldsResult_1_0_select_error_(&result);
        result._error._error = prusa3d_nfc_util_ReaderError_1_0_OTHER;
        return;
    }

    const auto io_result = reader_.enumerate_fields(static_cast<TagID>(request.tag.value), static_cast<Section>(request.section.value), result.fields.value.elements);
    if (!io_result) {
        prusa3d_nfc_request_EnumerateFieldsResult_1_0_select_error_(&result);
        result._error._error = std::to_underlying(io_result.error());
        return;
    }

    prusa3d_nfc_request_EnumerateFieldsResult_1_0_select_fields_(&result);
    result.fields.value.count = io_result->size();
}

void NFCTask::handle_raw_read_request(const prusa3d_nfc_request_RawRead_1_0 &request, prusa3d_nfc_request_RawReadResult_1_0 &result) {
    if (!radio_enabled_) {
        prusa3d_nfc_request_RawReadResult_1_0_select_error_(&result);
        result._error._error = prusa3d_nfc_util_ReaderError_1_0_RADIO_DISABLED;
        return;
    }

    if (request.num_bytes > std::size(result.data.value.elements)) {
        prusa3d_nfc_request_RawReadResult_1_0_select_error_(&result);
        result._error._error = prusa3d_nfc_util_ReaderError_1_0_DATA_TOO_BIG;
        return;
    }

    prusa3d_nfc_request_RawReadResult_1_0_select_data_(&result);
    result.data.value.count = request.num_bytes;

    const auto io_result = reader_.backend().read(request.tag.value, request.offset, std::span(reinterpret_cast<std::byte *>(result.data.value.elements), request.num_bytes));
    if (!io_result) {
        prusa3d_nfc_request_RawReadResult_1_0_select_error_(&result);
        result._error._error = std::to_underlying(io_result.error());
    }
}

void NFCTask::handle_raw_write_request(const prusa3d_nfc_request_RawWrite_1_0 &request, prusa3d_nfc_util_ReaderError_1_0 &result) {
    if (!radio_enabled_) {
        result._error = prusa3d_nfc_util_ReaderError_1_0_RADIO_DISABLED;
        return;
    }

    // Who knows what we are writing to the tag - invalidate higher-level reader cache
    reader_.invalidate_cache(request.tag.value);

    const auto io_result = reader_.backend().write(request.tag.value, request.offset, std::span(reinterpret_cast<const std::byte *>(request.data.value.elements), request.data.value.count));
    result._error = io_result_to_error(io_result);
}

void NFCTask::handle_initialize_tag_request(const prusa3d_nfc_request_InitializeTag_1_0 &request, prusa3d_nfc_util_ReaderError_1_0 &result) {
    if (!radio_enabled_) {
        result._error = prusa3d_nfc_util_ReaderError_1_0_RADIO_DISABLED;
        return;
    }

    // We're circumventing the high-level reader here, so invalidate its caches, things might have changed
    reader_.invalidate_cache(request.tag.value);

    const OPTBackend::InitializeTagParams params {
        .password = std::bit_cast<uint32_t>(request.password),
        .protect_first_num_bytes = request.protect_first_num_bytes,
        .protection_policy = static_cast<OPTBackend::InitializeTagParams::ProtectionPolicy>(request.protection_policy),
        .best_effort = request.best_effort,
    };
    const auto io_result = reader_.backend().initialize_tag(request.tag.value, params);
    result._error = io_result_to_error(io_result);
}

void NFCTask::handle_unlock_tag_request(const prusa3d_nfc_request_UnlockTag_1_0 &request, prusa3d_nfc_util_ReaderError_1_0 &result) {
    if (!radio_enabled_) {
        result._error = prusa3d_nfc_util_ReaderError_1_0_RADIO_DISABLED;
        return;
    }

    const auto io_result = reader_.backend().unlock_tag(request.tag.value, std::bit_cast<uint32_t>(request.password));
    result._error = io_result_to_error(io_result);
}

void NFCTask::handle_set_debug_config_request(const prusa3d_nfc_request_SetDebugConfig_1_0 &request) {
    const OPTBackend::DebugConfig config {
        .auto_forget_tag = request.auto_forget_tag,
    };
    reader_.backend().enforce_antenna(request.enforce_antenna);
    reader_.backend().set_debug_config(config);

    if (request.modulation_settings.count == 1) {
        hw_reconfiguration_callback_(request.modulation_settings.elements[0]);
        reader_.backend().reset_state();
    }
}

void NFCTask::handle_get_tag_uid_request(const prusa3d_nfc_request_GetTagUID_1_0 &request, prusa3d_nfc_request_GetTagUIDResult_1_0 &result) {
    // Note: Current implementation doesn't require radio for obtaining the UID, but some might in the future
    if (!radio_enabled_) {
        prusa3d_nfc_request_GetTagUIDResult_1_0_select_error_(&result);
        result._error._error = prusa3d_nfc_util_ReaderError_1_0_RADIO_DISABLED;
        return;
    }

    prusa3d_nfc_request_GetTagUIDResult_1_0_select_uid_(&result);
    const auto io_result = reader_.backend().get_tag_uid(request.tag.value, std::span { reinterpret_cast<std::byte *>(&result.uid.value.elements), sizeof(result.uid.value.elements) });
    if (!io_result) {
        prusa3d_nfc_request_GetTagUIDResult_1_0_select_error_(&result);
        result._error._error = io_result_to_error(io_result);
        return;
    }

    result.uid.value.count = *io_result;
    return;
}
