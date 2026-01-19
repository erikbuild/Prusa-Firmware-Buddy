#pragma once

namespace buddy::puppies {

/// Initialize and start task taking care of the puppies via Modbus
void start_puppy_task();

/// Suspend puppy task
void suspend_puppy_task();

} // namespace buddy::puppies
