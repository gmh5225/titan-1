#include "binary.hpp"
#include "logger.hpp"

#include <llvm/Support/CommandLine.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Support/Error.h>

llvm::cl::opt<std::string> binarypath("b",
    llvm::cl::desc("Path to the target Binary"),
    llvm::cl::value_desc("Binary"),
    llvm::cl::Required);

Binary::Binary()
{
    auto object_or_err = llvm::object::ObjectFile::createObjectFile(binarypath);

    if (object_or_err.takeError())
    {
        logger::error("Binary::Binary: Failed to create object file.");
    }

    std::tie(object, memory) = object_or_err->takeBinary();
}

std::optional<llvm::object::SectionRef> Binary::get_section(uint64_t address) const noexcept
{
    for (const auto& section : object->sections())
    {
        if (address >= section.getAddress() && address < section.getAddress() + section.getSize())
        {
            return section;
        }
    }
    return std::nullopt;
}

std::vector<uint8_t> Binary::get_bytes(uint64_t address, size_t size) const noexcept
{
    std::vector<uint8_t> raw;
    if (auto section = get_section(address))
    {
        if (auto offset = address - section->getAddress(); section->getSize() >= offset + size)
        {
            if (auto contents = section->getContents())
            {
                for (auto byte : contents->substr(offset, size))
                {
                    raw.push_back(byte);
                }
            }
            else
            {
                logger::info("Binary::get_bytes: Failed to read {} bytes from 0x{:x}.", size, address);
            }
        }
        else
        {
            logger::info("Binary::get_bytes: No offset within section for 0x{:x}:{} was found.", address, size);
        }
    }
    return raw;
}

bool Binary::is_x64() const noexcept
{
    return object->getArch() == llvm::Triple::ArchType::x86_64;
}
