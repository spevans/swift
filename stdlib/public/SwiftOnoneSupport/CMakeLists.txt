add_swift_library(swiftSwiftOnoneSupport SHARED IS_STDLIB
  # This file should be listed the first.  Module name is inferred from the
  # filename.
  SwiftOnoneSupport.swift
  SWIFT_COMPILE_FLAGS ${STDLIB_SIL_SERIALIZE_ALL} ${STDLIB_DISABLE_REDZONE} "-parse-stdlib"
  INSTALL_IN_COMPONENT stdlib)
