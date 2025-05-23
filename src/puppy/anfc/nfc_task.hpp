/// \file
#pragma once

#include <utils/atomic_circular_queue.hpp>
#include <move_only_inplace_function.hpp>
#include <nfc_prusa/prusa_nfc_reader.hpp>
#include <nfc_ll/ll_nfc_reader.hpp>

#include <freertos/mutex.hpp>
#include <o1heap/o1heap.hpp>

#include <prusa3d/nfc/command/Request_1_0.h>
#include <prusa3d/nfc/request/RequestResult_1_0.h>

/// Task that is responsible for handling NFC-related requests
/// Processing the requests can take quite some time, so we need a separate task
class NFCTask {

public:
    using Job = stdext::move_only_inplace_function<void()>;

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
    void handle_event(const PrusaNFCReader::Event &event);

    void handle_read_field_request(const prusa3d_nfc_request_ReadField_1_0 &request, prusa3d_nfc_util_ValueOrError_1_0 &result);
    void handle_write_field_request(const prusa3d_nfc_request_WriteField_1_0 &request, prusa3d_nfc_util_ReaderError_1_0 &result);

private:
    AtomicCircularQueue<Job, uint8_t, 32> job_queue_;

    /// Jobs can need some additional data to keep within themselves (typically serialized request)
    /// Don't forget to cover enqueues/frees with the mutex
    O1Heap<1024> job_queue_data_heap_;

    /// Mutex for enqueing the jobs and for the heap ops
    freertos::Mutex job_queue_mutex_;

    LLNFCReader ll_reader_;
    PrusaNFCReader reader_ { ll_reader_ };

    std::array<char, 256> mime_type_buffer_;
};

/// Defined in main.cpp
extern NFCTask nfc_task;
