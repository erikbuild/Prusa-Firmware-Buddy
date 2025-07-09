#pragma once

#include <option/has_e2ee_support.h>
#if HAS_E2EE_SUPPORT()
    #include <e2ee/identity_check_levels.hpp>
#endif
#include <cstdint>
#include <array>

// Mocked config store with the one variable we need. Not persistent.

struct ConfigStore {
    struct HostName {
        void set(const char *n) {
        }
        const char *get() const {
            return "nice_hostname";
        }
        const char *get_c_str() const {
            return "nice_hostname";
        }
    };

    struct ProxyHost {
        std::array<char, 10> get() const {
            return {};
        }
    };

    struct BoolTrue {
        bool get() const {
            return true;
        }
    };

    struct IntZero {
        int get() const {
            return 0;
        }
    };

    HostName hostname;
    BoolTrue verify_gcode;

    ProxyHost connect_proxy_host;
    IntZero connect_proxy_port;
#if HAS_E2EE_SUPPORT()
    struct Value {
        void set(e2ee::IdentityCheckLevel n) {
        }
        e2ee::IdentityCheckLevel get() const {
            return e2ee::IdentityCheckLevel::AnyIdentity;
        }
    };

    Value identity_check;
#endif
};

inline ConfigStore &config_store() {
    static ConfigStore store;
    return store;
}
