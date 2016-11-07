//===--- ImageInspectionStatic.cpp ----------------------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2016 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
// Implementation of functions to read data sections from static executable.
//
//===----------------------------------------------------------------------===//

// Currently only tested on linux but should work for any ELF platform
#if defined(__ELF__) && defined(__linux__)

#include "ImageInspection.h"

using namespace swift;

static SectionInfo protocolConformances;
static SectionInfo typeMetadata;

// Called from ImageInspectionInit.cpp
void
swift::addImageProtocolConformanceBlock(const SectionInfo block) {
  protocolConformances = block;
}

void
swift::addImageTypeMetadataRecordBlock(const SectionInfo block) {
  typeMetadata = block;
}

void
swift::initializeProtocolConformanceLookup() {
  addImageProtocolConformanceBlockCallback(protocolConformances.data,
                                           protocolConformances.size);
}

void
swift::initializeTypeMetadataRecordLookup() {
  addImageTypeMetadataRecordBlockCallback(typeMetadata.data,
                                          typeMetadata.size);
}

// This is called from Errors.cpp when dumping a stack trace entry.
// It could be implemented by parsing the ELF information in the
// executable. For now it returns 0 for error (cant lookup address).
int
swift::lookupSymbol(const void *address, SymbolInfo *info) {
  return 0;
}

#endif
