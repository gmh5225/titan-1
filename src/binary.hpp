#pragma once
#include <llvm/ADT/StringRef.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Support/MemoryBuffer.h>

#include <memory>
#include <string>
#include <cstdint>
#include <optional>

struct Binary
{
    Binary();

    auto get_section(uint64_t address)            const noexcept -> std::optional<llvm::object::SectionRef>;
    auto get_bytes(uint64_t address, size_t size) const noexcept -> std::vector<uint8_t>;

    template<typename T>
    std::vector<uint8_t> get_bytes(uint64_t address) const noexcept
    {
        return get_bytes(address, sizeof(T));
    }

    bool is_x64() const noexcept;

    auto begin() { return object->section_begin(); }
    auto end()   { return object->section_end();   }

private:
    std::unique_ptr<llvm::object::ObjectFile> object;
    std::unique_ptr<llvm::MemoryBuffer> memory;
};
