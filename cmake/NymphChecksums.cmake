# MIT License © 2026 Binary Dice Games
#
# Script mode: writes DIR/SHA256SUMS for every other file in DIR, in the
# format of `sha256sum` ("<hash>  <name>"), sorted by name, so
# `sha256sum -c SHA256SUMS` checks them.
#
#   cmake -DDIR=<dist folder> -P NymphChecksums.cmake

if(NOT DEFINED DIR)
  message(FATAL_ERROR "NymphChecksums.cmake: DIR is not set")
endif()
file(GLOB files RELATIVE "${DIR}" "${DIR}/*")
list(SORT files)
set(sums "")
foreach(name IN LISTS files)
  if(name STREQUAL "SHA256SUMS" OR IS_DIRECTORY "${DIR}/${name}")
    continue()
  endif()
  file(SHA256 "${DIR}/${name}" hash)
  string(APPEND sums "${hash}  ${name}\n")
endforeach()
file(WRITE "${DIR}/SHA256SUMS" "${sums}")
