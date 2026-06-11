/// @file
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include <resources/tarball.hpp>

namespace buddy::resources::tarball {

// ustar header field offsets and lengths.
inline constexpr size_t field_offset_name = 0;
inline constexpr size_t field_length_name = 100;
inline constexpr size_t field_offset_size = 124;
inline constexpr size_t field_length_size = 12;
inline constexpr size_t field_offset_chksum = 148;
inline constexpr size_t field_length_chksum = 8;
inline constexpr size_t field_offset_typeflag = 156;
inline constexpr size_t field_length_typeflag = 1;
inline constexpr size_t field_offset_magic = 257;
inline constexpr size_t field_length_magic = 6;
inline constexpr size_t field_offset_prefix = 345;
inline constexpr size_t field_length_prefix = 155;

/// Parse a space/NUL padded octal field. Returns false on a stray character.
[[nodiscard]] bool parse_octal(const uint8_t *field, size_t len, uint32_t &out);

/// Verify the ustar checksum of a `block_size`-byte header block.
[[nodiscard]] bool checksum_ok(const uint8_t *block);

/// Round up to a whole tar block.
[[nodiscard]] size_t padded_to_block(size_t n);

/// Validate the member name and build its absolute destination path,
/// rewritten in place in the header block.
/// Returns the path, or nullptr if the name is unsafe.
[[nodiscard]] const char *build_path(std::span<uint8_t> header, const char *root, bool is_dir);

} // namespace buddy::resources::tarball
