#pragma once

class PreciseStepping {

public:
    static void quick_stop() {}

    static void loop() {}

    static bool stopping() {
        return false;
    }
};
