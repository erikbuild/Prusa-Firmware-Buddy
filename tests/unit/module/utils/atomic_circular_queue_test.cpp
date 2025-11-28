#include <catch2/catch_test_macros.hpp>
#include <stdexcept>
#define ACQ_ASSERT(cond)                                      \
    if (!(cond)) {                                            \
        throw std::runtime_error("Assertion failed: " #cond); \
    }
#include <utils/atomic_circular_queue.hpp>

TEST_CASE("atomic_circular_queue", "[atomic_circular_queue]") {
    AtomicCircularQueue<uint8_t, size_t, 8> queue;

    uint8_t in = 0;
    uint8_t out = 0;

    // Start empty
    REQUIRE(queue.isEmpty() == true);
    REQUIRE(queue.isFull() == false);
    REQUIRE(queue.size() == 8);
    REQUIRE(queue.count() == 0);

    // Fill up
    for (size_t i = 0; i < 7; i++) {
        REQUIRE(queue.enqueue(in++) == true);
        REQUIRE(queue.isEmpty() == false);
        REQUIRE(queue.isFull() == false);
        REQUIRE(queue.count() == i + 1);
    }

    // Last in
    REQUIRE(queue.enqueue(in++) == true);
    REQUIRE(queue.isEmpty() == false);
    REQUIRE(queue.isFull() == true);
    REQUIRE(queue.count() == 8);

    // One more won't fit
    REQUIRE(queue.enqueue(in++) == false);
    REQUIRE(queue.isEmpty() == false);
    REQUIRE(queue.isFull() == true);
    REQUIRE(queue.count() == 8);

    // Take and fill few times around
    for (size_t i = 0; i < 36; i++) {
        uint8_t dequeued = 255;
        REQUIRE(queue.dequeue(dequeued) == true);
        REQUIRE(dequeued == out++);
        REQUIRE(queue.count() == 7);
        REQUIRE(queue.isEmpty() == false);
        REQUIRE(queue.isFull() == false);
        REQUIRE(queue.enqueue(8 + i) == true);
        REQUIRE(queue.isEmpty() == false);
        REQUIRE(queue.isFull() == true);
        REQUIRE(queue.count() == 8);
    }

    // Take out except for last
    for (size_t i = 0; i < 7; i++) {
        uint8_t dequeued = 255;
        REQUIRE(queue.dequeue(dequeued) == true);
        REQUIRE(dequeued == out++);
        REQUIRE(queue.isEmpty() == false);
        REQUIRE(queue.isFull() == false);
        REQUIRE(queue.count() == 7 - i);
    }

    // Take last
    REQUIRE(queue.count() == 1);
    uint8_t dequeued = 255;
    REQUIRE(queue.dequeue(dequeued) == true);
    REQUIRE(dequeued == out++);
    REQUIRE(queue.isEmpty() == true);
    REQUIRE(queue.isFull() == false);
    REQUIRE(queue.count() == 0);
    REQUIRE(queue.dequeue(dequeued) == false);
}

TEST_CASE("atomic_circular_reservable_queue", "[atomic_circular_queue]") {
    AtomicReservableCircularQueue<uint8_t, size_t, 8> queue;

    SECTION("Initial state") {
        REQUIRE(queue.isEmpty() == true);
        REQUIRE(queue.isFull() == false);
        REQUIRE(queue.size() == 8);
        REQUIRE(queue.count() == 0);
    }

    SECTION("Valid access") {
        uint8_t *allocated = queue.allocate();
        *allocated = 31;
        REQUIRE_NOTHROW(queue.commit(allocated));

        uint8_t dequeued;
        REQUIRE(queue.dequeue(dequeued) == true);
        REQUIRE(dequeued == 31);
    }

    SECTION("Commit without an allocated item") {
        uint8_t not_allocated = 0;
        queue.allocate();
#ifndef NDEBUG
        REQUIRE_THROWS(queue.commit(&not_allocated));
#endif
    }

    SECTION("Allocate two times") {
        queue.allocate();
#ifndef NDEBUG
        REQUIRE_THROWS(queue.allocate());
#endif
    }

    SECTION("Fill up the queue") {
        for (uint8_t i = 0; i < 7; i++) {
            uint8_t *to_modify = queue.allocate();
            *to_modify = 1;
            REQUIRE_NOTHROW(queue.commit(to_modify));
        }
        REQUIRE(queue.count() == 7);
        REQUIRE(queue.isFull() == false);
        REQUIRE(queue.isEmpty() == false);

        // Commit last
        uint8_t *last = queue.allocate();
        *last = 1;
        REQUIRE_NOTHROW(queue.commit(last));

        // Commit one more (should fail)
        uint8_t *one_more = queue.allocate();
        REQUIRE(one_more == nullptr);

        REQUIRE(queue.count() == 8);
        REQUIRE(queue.isFull() == true);

        for (uint i = 0; i < 8; i++) {
            uint8_t t;
            REQUIRE(queue.dequeue(t) == true);
            REQUIRE(t == 1);
        }
        REQUIRE(queue.isEmpty() == true);
        REQUIRE(queue.isFull() == false);
        REQUIRE(queue.count() == 0);
    }
}
