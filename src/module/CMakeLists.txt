# The `src/module` directory is dedicated to only "nice" libraries (defined through add_library). No
# `firmware` targets and similar. Keep it civil, please

add_subdirectory(utils)
add_subdirectory(magic_optimizations)
add_subdirectory(raii)
add_subdirectory(nfc)
