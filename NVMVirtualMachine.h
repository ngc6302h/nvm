#pragma once
#include <Span.h>
#include <Vector.h>
#include <Hashmap.h>

namespace nvm
{
    class NVMMemory
    {
    public:
        explicit NVMMemory(u64 chunk_size) : m_chunk_size(chunk_size), m_chunks()
        {
            m_chunks.insert(0, (u8*) calloc(chunk_size, 1));
        }
    
        u64 read_8(u64 address)
        {
            auto maybe_chunk = m_chunks.get(address - (address % m_chunk_size));
            if (!maybe_chunk.has_value())
            {
                m_chunks.insert(address - (address % m_chunk_size), (u8*) calloc(m_chunk_size, 1));
                return 0; //skip fetching chunk. since newly used memory is blank, we can just return 0
            }
            else
                return maybe_chunk.value()[address % m_chunk_size];
        }
        
        u64 read_16(u64 address)
        {
            auto maybe_chunk = m_chunks.get(address - (address % m_chunk_size));
            if (!maybe_chunk.has_value())
            {
                m_chunks.insert(address - (address % m_chunk_size), (u8*) calloc(m_chunk_size, 1));
                maybe_chunk = m_chunks.get(address - (address % m_chunk_size));
            }
    
            auto chunk = maybe_chunk.value();
            if (m_chunk_size - (address % m_chunk_size) >= 2)
            {
                return *reinterpret_cast<u16*>(chunk+(address%m_chunk_size));
            }
            else
            {
                return chunk[address%m_chunk_size] | (read_8(address+1) << 8);
            }
        }
        
        u64 read_32(u64 address)
        {
            auto maybe_chunk = m_chunks.get(address - (address % m_chunk_size));
            if (!maybe_chunk.has_value())
            {
                m_chunks.insert(address - (address % m_chunk_size), (u8*) calloc(m_chunk_size, 1));
                maybe_chunk = m_chunks.get(address - (address % m_chunk_size));
            }
    
            auto chunk = maybe_chunk.value();
            if (m_chunk_size - (address % m_chunk_size) >= 4)
            {
                return *reinterpret_cast<u32*>(chunk+(address%m_chunk_size));
            }
            else
            {
                if (m_chunk_size - (address % m_chunk_size) >= 2)
                    return (*reinterpret_cast<u16*>(chunk + (address % m_chunk_size))) | (read_16(address+2) << 16);
                else
                    return chunk[address%m_chunk_size] | (read_8(address+1) << 8) | (read_16(address+2) << 16);
            }
        }
        
        u64 read_64(u64 address)
        {
            auto maybe_chunk = m_chunks.get(address - (address % m_chunk_size));
            if (!maybe_chunk.has_value())
            {
                m_chunks.insert(address - (address % m_chunk_size), (u8*) calloc(m_chunk_size, 1));
                maybe_chunk = m_chunks.get(address - (address % m_chunk_size));
            }
    
            auto chunk = maybe_chunk.value();
            if (m_chunk_size - (address % m_chunk_size) >= 8)
            {
                return *reinterpret_cast<u64*>(chunk+(address%m_chunk_size));
            }
            else
            {
                if (m_chunk_size - (address % m_chunk_size) >= 4)
                    return (*reinterpret_cast<u32*>(chunk + (address % m_chunk_size))) | (read_32(address+2) << 32);
                else if (m_chunk_size - (address % m_chunk_size) >= 2)
                    return (*reinterpret_cast<u16*>(chunk + (address % m_chunk_size))) | (read_16(address+2) << 16) | (read_32(address+2) << 32);
                else
                    return chunk[address%m_chunk_size] | (read_8(address+1) << 8) | (read_16(address+2) << 16) | (read_32(address+4) << 32);
            }
        }
        
        void write_8(u64 address, u8 value)
        {
            auto maybe_chunk = m_chunks.get(address - (address % m_chunk_size));
            if (!maybe_chunk.has_value())
            {
                auto chunk = (u8*) calloc(m_chunk_size, 1);
                m_chunks.insert(address - (address % m_chunk_size), chunk);
                chunk[address % m_chunk_size] = value;
            }
            else
                maybe_chunk.value()[address % m_chunk_size] = value;
        }
        
        void write_16(u64 address, u16 value)
        {
            auto maybe_chunk = m_chunks.get(address - (address % m_chunk_size));
            if (!maybe_chunk.has_value())
            {
                m_chunks.insert(address - (address % m_chunk_size), (u8*) calloc(m_chunk_size, 1));
                maybe_chunk = m_chunks.get(address - (address % m_chunk_size));
            }
    
            auto chunk = maybe_chunk.value();
            if (m_chunk_size - (address % m_chunk_size) >= 2)
            {
                 *reinterpret_cast<u16*>(chunk+(address%m_chunk_size)) = value;
            }
            else
            {
                 chunk[address%m_chunk_size] = value & 0xFF;
                 write_8(address+1, (value & 0xFF00) >> 8);
            }
        }
        
        void write_32(u64 address, u32 value)
        {
            auto maybe_chunk = m_chunks.get(address - (address % m_chunk_size));
            if (!maybe_chunk.has_value())
            {
                m_chunks.insert(address - (address % m_chunk_size), (u8*) calloc(m_chunk_size, 1));
                maybe_chunk = m_chunks.get(address - (address % m_chunk_size));
            }
    
            auto chunk = maybe_chunk.value();
            if (m_chunk_size - (address % m_chunk_size) >= 4)
            {
                *reinterpret_cast<u32*>(chunk+(address%m_chunk_size)) = value;
            }
            else if (m_chunk_size - (address % m_chunk_size) >= 2)
            {
                *reinterpret_cast<u16*>(chunk+(address%m_chunk_size)) = value & 0xFFFF;
                write_16(address+2, (value & 0xFFFF0000) >> 16);
            }
            else
            {
                chunk[address % m_chunk_size] = value & 0xFF;
                write_8(address+1, (value & 0xFF00) >> 8);
                write_16(address+2, (value & 0xFFFF0000) >> 16);
            }
        }
    
        void write_64(u64 address, u64 value)
        {
            auto maybe_chunk = m_chunks.get(address - (address % m_chunk_size));
            if (!maybe_chunk.has_value())
            {
                m_chunks.insert(address - (address % m_chunk_size), (u8*) calloc(m_chunk_size, 1));
                maybe_chunk = m_chunks.get(address - (address % m_chunk_size));
            }
        
            auto chunk = maybe_chunk.value();
            if (m_chunk_size - (address % m_chunk_size) >= 8)
            {
                *reinterpret_cast<u64*>(chunk+(address%m_chunk_size)) = value;
            }
            else if (m_chunk_size - (address % m_chunk_size) >= 4)
            {
                *reinterpret_cast<u32*>(chunk+(address%m_chunk_size)) = value & 0xFFFFFFFF;
                write_32(address+4, (value & 0xFFFFFFFF00000000) >> 32);
            }
            else if (m_chunk_size - (address % m_chunk_size) >= 2)
            {
                *reinterpret_cast<u16*>(chunk+(address%m_chunk_size)) = value & 0xFFFF;
                write_16(address+2, (value & 0xFFFF0000) >> 16);
                write_32(address+4, (value & 0xFFFFFFFF00000000) >> 32);
            }
            else
            {
                chunk[address % m_chunk_size] = value & 0xFF;
                write_8(address+1, (value & 0xFF00) >> 8);
                write_16(address+2, (value & 0xFFFF0000) >> 16);
                write_32(address+4, (value & 0xFFFFFFFF00000000) >> 32);
            }
        }
        
    private:
        u64 m_chunk_size;
        Hashmap<u64, u8*> m_chunks;
        u64 registers[11] { 0 };
    };
    
    using ExitCode = u64;
    class NVMVirtualMachine
    {
    public:
        explicit NVMVirtualMachine(const Span<u8>& bytecode);
        ExitCode run();
        
    private:
        NVMMemory m_memory;
    };
}
