#pragma once

#include <nanocbor/nanocbor.h>

/// (Recursively) copies value from source to target
int nanocbor_copy_value(nanocbor_value_t *src, nanocbor_encoder_t *tgt);
