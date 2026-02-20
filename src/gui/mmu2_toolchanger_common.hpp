#pragma once

/**
 * @file mmu2_toolchanger_common.hpp
 * @brief It includes required headers and function declerations for different implementations (XL / MK4 with MMU2) of spool join
 */

#include <option/has_mmu2.h>
#if HAS_MMU2()
    #include <feature/prusa/MMU2/mmu2_mk4.h>
#endif
