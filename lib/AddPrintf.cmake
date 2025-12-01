add_library(printf printf/printf.c)
target_include_directories(printf PUBLIC printf)
# Can't compile with LTO, because linker can't keep __putc syscall even with __attribute((used))
set_property(TARGET printf PROPERTY INTERPROCEDURAL_OPTIMIZATION OFF)
target_link_options(
  printf
  PUBLIC
  -Wl,--defsym=printf=printf_,--defsym=sprintf=sprintf_,--defsym=snprintf=snprintf_,--defsym=vprintf=vprintf_,--defsym=vsnprintf=vsnprintf_
  )
add_library(printf::printf ALIAS printf)
