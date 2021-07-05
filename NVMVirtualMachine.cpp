#include "NVMVirtualMachine.h"
#include "NVMData.h"

namespace nvm
{
    NVMVirtualMachine::NVMVirtualMachine(const Span<u8>& bytecode) : m_memory(32*1024)
    {
        //this loads a raw blob of instructions starting in 0x0. for specific relocation loads, see formatted load (TODO)
        u64 load_address = 0;
        if (bytecode.size() %  8 == 0)
        {
            for (auto w : bytecode.as<u64>())
            {
                m_memory.write_64(load_address, w);
                load_address += 8;
            }
        }
        else if (bytecode.size() % 4 == 0)
        {
            for (auto w : bytecode.as<u32>())
            {
                m_memory.write_32(load_address, w);
                load_address += 4;
            }
        }
        else if (bytecode.size() % 2 == 0)
        {
            for (auto w : bytecode.as<u16>())
            {
                m_memory.write_16(load_address, w);
                load_address += 2;
            }
        }
        else
        {
            for (auto w : bytecode)
            {
                m_memory.write_32(load_address++, w);
            }
        }
    }
    
    ExitCode NVMVirtualMachine::run()
    {
        
        return 0;
    }
}