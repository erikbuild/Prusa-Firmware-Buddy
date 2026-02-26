#include "store_instance.hpp"
#include <option/has_config_store_wo_backend.h>

using namespace config_store_ns;

void init_config_store() {
#if HAS_CONFIG_STORE_WO_BACKEND()
    config_store(); // called to ensure object creation
    config_store_init_result() = config_store_ns::InitResult::normal;
#else
    config_store().init(); // initializes the store's backend, will be a cold start
    config_store().load_all(); // loads the config_store from the one transaction (can trigger further config_store migrations)
    const auto journal_state = config_store().get_backend().get_journal_state();

    if (journal_state == journal::Backend::JournalState::ColdStart) {
        config_store_init_result() = config_store_ns::InitResult::cold_start;
    } else {
        config_store_init_result() = config_store_ns::InitResult::normal;
    }
#endif
}
