//===-- StaticBinaryELF.cpp -------------------------------------*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2016 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See http://swift.org/LICENSE.txt for license information
// See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
// Parse a static ELF binary to implement _swift_dladdr() address lookup.
//
//===----------------------------------------------------------------------===//

#if defined(__ELF__) && defined(__linux__)

#include "ImageInspection.h"
#include <string>
#include <vector>
#include <elf.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <linux/limits.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

using namespace std;

#ifdef __LP64__
#define ELFCLASS ELFCLASS64
typedef Elf64_Ehdr Elf_Ehdr;
typedef Elf64_Shdr Elf_Shdr;
typedef Elf64_Phdr Elf_Phdr;
typedef Elf64_Addr Elf_Addr;
typedef Elf64_Word Elf_Word;
typedef Elf64_Sym  Elf_Sym;
typedef Elf64_Section Elf_Section;
#define ELF_ST_TYPE(x) ELF64_ST_TYPE(x)
#else
#define ELFCLASS ELFCLASS32
typedef Elf32_Ehdr Elf_Ehdr;
typedef Elf32_Shdr Elf_Shdr;
typedef Elf32_Phdr Elf_Phdr;
typedef Elf32_Addr Elf_Addr;
typedef Elf32_Word Elf_Word;
typedef Elf32_Sym  Elf_Sym;
typedef Elf32_Section Elf_Section;
#define ELF_ST_TYPE(x) ELF32_ST_TYPE(x)
#endif

struct StaticBinaryELF {

private:
  string fullPathName;
  size_t mapLength = 0;
  size_t fileSize = 0;
  const char *mapping = nullptr;
  const Elf_Ehdr *elfHeader = nullptr;
  const Elf_Shdr *symbolTable = nullptr;
  const Elf_Shdr *stringTable = nullptr;

public:
  StaticBinaryELF(const char *filename) {
    executablePathName(filename);
    mmapExecutable(filename);
  }

  ~StaticBinaryELF() {
    if (mapping != nullptr) {
      munmap(const_cast<char *>(mapping), mapLength);
    }
  }

  const char *getPathName() {
    return fullPathName.c_str();
  }

  void *getSectionLoadAddress(const void *addr) {
    if (elfHeader != nullptr) {
      auto searchAddr = reinterpret_cast<Elf_Addr>(addr);
      auto headers = reinterpret_cast<const Elf_Phdr *>(mapping +
                                                        elfHeader->e_phoff);

      for (size_t idx = 0; idx < elfHeader->e_phnum; idx++) {
        auto header = &headers[idx];
        if (header->p_type == PT_LOAD && searchAddr >= header->p_vaddr
            && searchAddr <= (header->p_vaddr + header->p_memsz)) {
          return reinterpret_cast<void *>(header->p_vaddr);
        }
      }
    }
    return nullptr;
  }

  // Lookup a function symbol by address
  const Elf_Sym *findSymbol(const void *addr) {
    if (symbolTable != nullptr) {
      auto searchAddr = reinterpret_cast<Elf_Addr>(addr);
      auto entries = symbolTable->sh_size / symbolTable->sh_entsize;
      auto symbols = reinterpret_cast<const Elf_Sym *>(mapping +
                                                       symbolTable->sh_offset);

      for (decltype(entries) idx = 0; idx < entries; idx++) {
        auto symbol = &symbols[idx];
        if (ELF_ST_TYPE(symbol->st_info) == STT_FUNC
            && searchAddr >= symbol->st_value
            && searchAddr < (symbol->st_value + symbol->st_size)) {
          return symbol;
        }
      }
    }
    return nullptr;
  }

  const char *symbolName(const Elf_Sym *symbol) {
    if (stringTable == nullptr || symbol->st_name >= stringTable->sh_size) {
      return nullptr;
    }
    return mapping + stringTable->sh_offset + symbol->st_name;
  }


private:
  // If the binary is a symlink (eg /proc/self/exe) resolve to the canonical
  // filename
  void executablePathName(const char *filename) {
    vector<char> fname(PATH_MAX);
    ssize_t ret = readlink(filename, &fname[0], PATH_MAX-1);
    if (ret == -1) {
      fullPathName = filename;
      return;
    }
    if (ret >= PATH_MAX) {
      ret = PATH_MAX;
    }
    fname[ret] = '\0';
    fullPathName = &fname[0];
  }

  // Parse the ELF binary using mmap to read it but keep the mapped region
  // as small as possible and expand it as necessary
  bool mmapExecutable(const char *filename) {
    struct stat buf;

    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
      return false;
    }

    if (fstat(fd, &buf) != 0) {
      close(fd);
      return false;
    }

    void *map = mmap(nullptr, sizeof(Elf_Ehdr), PROT_READ, MAP_SHARED, fd, 0);
    close(fd);
    if (map == MAP_FAILED) {
      return false;
    }
    fileSize = buf.st_size;
    mapping = reinterpret_cast<const char *>(map);
    mapLength = sizeof(Elf_Ehdr);

    if (!readElfHeader()) {
      munmap(map, mapLength);
      mapping = nullptr;
      mapLength = 0;
      elfHeader = nullptr;
      symbolTable = nullptr;
      stringTable = nullptr;
      return false;
    }
    return true;
  }

  // Read the ELF header and sections from the binary validating that is it
  // a static binary of the correct bitsize. Expand the mmap region as needed
  // but allow for some sections to be missing (eg if the binary has been
  // stripped). Whilst not ideal it does at least allow some information to be
  // used by dladdr()
  bool readElfHeader() {
    elfHeader = reinterpret_cast<const Elf_Ehdr *>(mapping);

    // Validate the header magic number
    if (elfHeader->e_ident[EI_MAG0] != ELFMAG0
        || elfHeader->e_ident[EI_MAG1] != ELFMAG1
        || elfHeader->e_ident[EI_MAG2] != ELFMAG2
        || elfHeader->e_ident[EI_MAG3] != ELFMAG3) {
      return false;
    }

    // Validate the binary is current ELF executable for the CPU
    // bitsize
    if (elfHeader->e_ident[EI_CLASS] != ELFCLASS
        || elfHeader->e_type != ET_EXEC
        || elfHeader->e_version != EV_CURRENT
        || elfHeader->e_ehsize != sizeof(Elf_Ehdr)) {
      return false;
    }

    // Map in the program header block
    auto programHeaderSize = (elfHeader->e_phentsize * elfHeader->e_phnum);
    if (!expandMapping(elfHeader->e_phoff + programHeaderSize)) {
      return false;
    }

    auto headers = reinterpret_cast<const Elf_Phdr *>(mapping +
                                                      elfHeader->e_phoff);
    // If a interpreter is set in the program headers then this is a
    // dynamic executable and therefore not valid
    for (size_t idx = 0; idx < elfHeader->e_phnum; idx++) {
      if (headers[idx].p_type == PT_INTERP) {
        return false;
      }
    }

    // Map in the section headers
    auto sectionHeaderSize = (elfHeader->e_shentsize * elfHeader->e_shnum);
    if (!expandMapping(elfHeader->e_shoff + sectionHeaderSize)) {
      return false;
    }
    symbolTable = findSection(SHT_SYMTAB);
    stringTable = findSection(SHT_STRTAB);

    return true;
  }

  // Find the section of a specified type expanding the mapping as needed
  const Elf_Shdr *findSection(Elf_Word sectionType) {
    auto headers = reinterpret_cast<const Elf_Shdr *>(mapping +
                                                      elfHeader->e_shoff);

    for (size_t idx = 0; idx < elfHeader->e_shnum; idx++) {
      if (idx == elfHeader->e_shstrndx) {
        continue;
      }
      auto header = &headers[idx];
      if (header->sh_type == sectionType) {
        if (header->sh_entsize > 0 && header->sh_size % header->sh_entsize) {
          fprintf(stderr,
                  "section size is not a multiple of entrysize (%ld/%ld)\n",
                  header->sh_size, header->sh_entsize);
          return nullptr;
        }

        if (!expandMapping(header->sh_offset + header->sh_size)) {
          return nullptr;
        }
        return header;
      }
    }
    return nullptr;
  }

  bool expandMapping(size_t size) {
    if (mapping == nullptr || size > fileSize) {
      return false;
    }
    if (size > mapLength) {
      void *map = mremap(const_cast<char *>(mapping), mapLength, size,
                         MREMAP_MAYMOVE);
      if (map == MAP_FAILED) {
        return false;
      }
      mapLength = size;
      mapping = reinterpret_cast<const char *>(map);
      elfHeader = reinterpret_cast<const Elf_Ehdr *>(map);
    }
    return true;
  }
};


int
swift::_swift_dladdr(const void* addr, Dl_info *info) {
  // The pointers returned point into the mmap()'d binary so keep the
  // object once instantiated
  static StaticBinaryELF *binary = nullptr;
  static pthread_once_t onceToken = PTHREAD_ONCE_INIT;

  pthread_once(&onceToken, []() {
      binary = new StaticBinaryELF("/proc/self/exe");
    });

  info->dli_fname = binary->getPathName();
  info->dli_fbase = binary->getSectionLoadAddress(addr);

  auto symbol = binary->findSymbol(addr);
  if (symbol != nullptr) {
    info->dli_saddr = reinterpret_cast<void *>(symbol->st_value);
    info->dli_sname = binary->symbolName(symbol);
  } else {
    info->dli_saddr = nullptr;
    info->dli_sname = nullptr;
  }

  return 1;
}

#endif // defined(__ELF__) && defined(__linux__)
