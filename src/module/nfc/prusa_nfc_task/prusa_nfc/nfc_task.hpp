/// \file
#pragma once

#define DO_NOT_CHECK_ATOMIC_LOCK_FREE

#include <utils/atomic_circular_queue.hpp>
#include <move_only_inplace_function.hpp>
#include <openprinttag/opt_reader.hpp>
#include <openprinttag/opt_backend.hpp>

#include <freertos/mutex.hpp>
#include <o1heap/o1heap.hpp>

#include <prusa3d/nfc/command/Request_1_0.h>
#include <prusa3d/nfc/request/RequestResult_1_0.h>
#include <prusa3d/nfc/event/Event_1_0.h>

/// Task that is responsible for handling NFC-related requests
/// Processing the requests can take quite some time, so we need a separate task
class NFCTask {

public:
    using Job = stdext::move_only_inplace_function<void()>;
    using EventCallback = stdext::inplace_function<void(prusa3d_nfc_event_Event_1_0 &)>;
    using HWReconfigurationCallback = stdext::inplace_function<void(const prusa3d_nfc_request_debug_ModulationConfig_1_0 &)>;

public:
    NFCTask(openprinttag::OPTBackend &backend, EventCallback &&event_callback, HWReconfigurationCallback &&hw_reconfiguration_callback);

public:
    /// Attempts to enqueue a request for processing
    /// \returns false if the queue is full
    [[nodiscard]] bool enqueue_serialized_request(const std::span<const uint8_t> &data);

public:
    /// Thread function, for internal use only
    void task();

private:
    /// Enqueues the job to be executed on the NFC task
    /// May block if the queue is full
    [[nodiscard]] bool enqueue_job(Job &&job);

private:
    void handle_event(const openprinttag::OPTReader::Event &event);

    void handle_read_field_request(const prusa3d_nfc_request_ReadField_1_0 &request, prusa3d_nfc_util_ValueOrError_1_0 &result);
    void handle_write_field_request(const prusa3d_nfc_request_WriteField_1_0 &request, prusa3d_nfc_util_ReaderError_1_0 &result);
    void handle_enumerate_fields_request(const prusa3d_nfc_request_EnumerateFields_1_0 &request, prusa3d_nfc_request_EnumerateFieldsResult_1_0 &result);
    void handle_raw_read_request(const prusa3d_nfc_request_RawRead_1_0 &request, prusa3d_nfc_request_RawReadResult_1_0 &result);
    void handle_raw_write_request(const prusa3d_nfc_request_RawWrite_1_0 &request, prusa3d_nfc_util_ReaderError_1_0 &result);
    void handle_initialize_tag_request(const prusa3d_nfc_request_InitializeTag_1_0 &request, prusa3d_nfc_util_ReaderError_1_0 &result);
    void handle_unlock_tag_request(const prusa3d_nfc_request_UnlockTag_1_0 &request, prusa3d_nfc_util_ReaderError_1_0 &result);
    void handle_set_debug_config_request(const prusa3d_nfc_request_SetDebugConfig_1_0 &request);
    void handle_get_tag_uid_request(const prusa3d_nfc_request_GetTagUID_1_0 &request, prusa3d_nfc_request_GetTagUIDResult_1_0 &result);

private:
    AtomicCircularQueue<Job, uint8_t, 16> job_queue_;

    /// Jobs can need some additional data to keep within themselves (typically serialized request)
    /// Don't forget to cover enqueues/frees with the mutex
    O1Heap<1024> job_queue_data_heap_;

    /// Mutex for enqueing the jobs and for the heap ops
    freertos::Mutex job_queue_mutex_;

    /// Callback for when NFCTask generates an event that should be broadcasted
    EventCallback event_callback_;

    /// Callback for when NFCTask receives request to reconfigure the ll nfc reader (debug only)
    HWReconfigurationCallback hw_reconfiguration_callback_;

    openprinttag::OPTReader reader_;

    std::array<char, 64> mime_type_buffer_;

    bool radio_enabled_ = false;
};
