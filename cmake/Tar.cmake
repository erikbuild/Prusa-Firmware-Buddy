set(mktar "${CMAKE_SOURCE_DIR}/utils/mktar.py")

# Declare a tar archive target. Files accumulate via tar_image_add_file(); call tar_image_build()
# once all files have been added. The archive path is exposed as TAR_IMAGE_LOCATION property.
function(add_tar_image image_name)
  set(image_location "${CMAKE_CURRENT_BINARY_DIR}/${image_name}.tar")
  add_custom_target(${image_name} DEPENDS "${image_location}")
  set_target_properties(${image_name} PROPERTIES TAR_IMAGE_LOCATION "${image_location}")
endfunction()

# Append a (target, source) pair to the archive.
function(tar_image_add_file image_name file target)
  get_filename_component(file "${file}" ABSOLUTE)
  set_property(
    TARGET ${image_name}
    APPEND
    PROPERTY TAR_IMAGE_ENTRIES "${target}" "${file}"
    )
  set_property(
    TARGET ${image_name}
    APPEND
    PROPERTY TAR_IMAGE_SOURCES "${file}"
    )
endfunction()

# Emit the build rule producing the archive from the accumulated entries.
function(tar_image_build image_name)
  get_target_property(output ${image_name} TAR_IMAGE_LOCATION)
  get_target_property(entries ${image_name} TAR_IMAGE_ENTRIES)
  get_target_property(sources ${image_name} TAR_IMAGE_SOURCES)
  add_custom_command(
    OUTPUT "${output}"
    COMMAND "${Python3_EXECUTABLE}" "${mktar}" "${output}" ${entries}
    DEPENDS ${sources} "${mktar}"
    VERBATIM
    )
endfunction()
