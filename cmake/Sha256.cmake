# Helper for computing the raw SHA256 digest of a file at build time.
set(mksha256 "${CMAKE_SOURCE_DIR}/utils/mksha256.py")

# Emit a build rule writing the raw 32-byte SHA256 digest of `input` to `output`.
function(sha256_file input output)
  add_custom_command(
    OUTPUT "${output}"
    COMMAND "${Python3_EXECUTABLE}" "${mksha256}" "${input}" "${output}"
    DEPENDS "${input}" "${mksha256}"
    VERBATIM
    )
endfunction()
