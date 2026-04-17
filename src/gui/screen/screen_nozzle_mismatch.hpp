/// @file
#pragma once

#include <screen_fsm.hpp>

class ScreenNozzleMismatch final : public ScreenFSM {
public:
    ScreenNozzleMismatch();
    ~ScreenNozzleMismatch();

protected:
    void create_frame() final;
    void destroy_frame() final;
    void update_frame() final;
};
