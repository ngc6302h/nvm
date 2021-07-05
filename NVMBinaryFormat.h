#pragma once
#include "Types.h"
#include "SmartPtr.h"
#include "Vector.h"
#include "ResultOrError.h"
#include "StringView.h"

namespace nvm
{
    struct NVMBinaryFormatData
    {
        u32 magic;
        u32 crc32;
        u64 load_offset;
        u64 entry_point;
        RefPtr<Vector<u8>> rom;
    };
    
    
    ResultOrError<NVMBinaryFormatData, StringView> try_read(const Span<u8>& data)
    {
        if (data.size() < sizeof(NVMBinaryFormatData))
            return "specified buffer isn't long enough for correct parsing"_sv;
        
        auto magic = data.as<u32>()[0];
        if (magic != 0x63026302)
            return "bad magic"_sv;
        auto crc32 = data.as<u32>()[1];
        auto load_offset = data.as<u64>().slice(1)[0];
        auto entry_point = data.as<u64>().slice(1)[1];
        
        RefPtr<Vector<u8>> rom(new Vector<u8>());
        for (auto byte : data.slice(sizeof(u32)*2+sizeof(u64)*2))
            rom->append(byte);
        
        //TODO: implement crc32 check
        return NVMBinaryFormatData { magic, crc32, load_offset, entry_point, move(rom) };
    }
    
    RefPtr<Vector<u8>> make_nvm_format(u64 load_offset, u64 entry_point, const Span<u8>& data)
    {
        RefPtr<Vector<u8>> buf(new Vector<u8>(24+data.size()));
        //insert magic
        buf->append(0x02);
        buf->append(0x63);
        buf->append(0x02);
        buf->append(0x63);
        //insert crc32
        buf->append(0);
        buf->append(0);
        buf->append(0);
        buf->append(0);
        //insert load offset
        for (int i = 0; i < sizeof(u64); i++)
            buf->append((load_offset & (0xFF << i*8)) >> i*8);
        //insert entry point
        for (int i = 0; i < sizeof(u64); i++)
            buf->append((entry_point & (0xFF << i*8)) >> i*8);
        return buf;
    }
}