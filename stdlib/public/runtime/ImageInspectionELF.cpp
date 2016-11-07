//===--- ImageInspectionELF.cpp - ELF image inspection --------------------===//
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
// This file includes routines that interact with ld*.so on ELF-based platforms
// to extract runtime metadata embedded in dynamically linked ELF images
// generated by the Swift compiler.
//
//===----------------------------------------------------------------------===//

#if defined(__ELF__) || defined(__ANDROID__)

#include "ImageInspection.h"
#include "../SwiftShims/Visibility.h"
#include "swift/Basic/Lazy.h"
#include "swift/Runtime/Mutex.h"
#include <cassert>
#include <vector>
#include <dlfcn.h>

using namespace swift;

static Mutex *protocolConformanceLock = nullptr;
static std::vector<SectionInfo> *protocolConformanceBlocks = nullptr;

static Mutex *typeMetadataLock = nullptr;
static std::vector<SectionInfo> *typeMetadataRecordBlocks = nullptr;


SWIFT_RUNTIME_EXPORT
void swift::addImageProtocolConformanceBlock(const SectionInfo block) {
  static OnceToken_t onceToken;
  SWIFT_ONCE_F(onceToken, [](void *context) {
      protocolConformanceBlocks = new std::vector<SectionInfo>();
      protocolConformanceLock = new Mutex();
    }, nullptr);

  if (block.size > 0) {
    ScopedLock guard(*protocolConformanceLock);
    if (protocolConformanceBlocks == nullptr) {
      addImageProtocolConformanceBlockCallback(block.data, block.size);
    } else {
      protocolConformanceBlocks->push_back(block);
    }
  }
}

void swift::initializeProtocolConformanceLookup() {
  assert(protocolConformanceLock != nullptr);
  ScopedLock guard(*protocolConformanceLock);
  if (protocolConformanceBlocks) {
    for (auto block: *protocolConformanceBlocks) {
      addImageProtocolConformanceBlockCallback(block.data, block.size);
    }
    delete protocolConformanceBlocks;
    protocolConformanceBlocks = nullptr;
  }
}

SWIFT_RUNTIME_EXPORT
void swift::addImageTypeMetadataRecordBlock(const SectionInfo block) {
  static OnceToken_t onceToken;
  SWIFT_ONCE_F(onceToken, [](void *context) {
      typeMetadataRecordBlocks = new std::vector<SectionInfo>();
      typeMetadataLock = new Mutex();
    }, nullptr);

  if (block.size > 0) {
    ScopedLock guard(*typeMetadataLock);
    if (typeMetadataRecordBlocks == nullptr) {
      addImageTypeMetadataRecordBlockCallback(block.data, block.size);
    } else {
      typeMetadataRecordBlocks->push_back(block);
    }
  }
}

void swift::initializeTypeMetadataRecordLookup() {
  assert(typeMetadataLock != nullptr);
  ScopedLock guard(*typeMetadataLock);
  if (typeMetadataRecordBlocks) {
    for (auto block: *typeMetadataRecordBlocks) {
      addImageTypeMetadataRecordBlockCallback(block.data, block.size);
    }
    delete typeMetadataRecordBlocks;
    typeMetadataRecordBlocks = nullptr;
  }
}

int swift::lookupSymbol(const void *address, SymbolInfo *info) {
  Dl_info dlinfo;
  if (dladdr(address, &dlinfo) == 0) {
    return 0;
  }

  info->fileName = dlinfo.dli_fname;
  info->baseAddress = dlinfo.dli_fbase;
  info->symbolName = dlinfo.dli_sname;
  info->symbolAddress = dlinfo.dli_saddr;
  return 1;
}

#endif // defined(__ELF__) || defined(__ANDROID__)
