#pragma once
#include "Array.h"
#include "NVMVirtualMachine.h"

namespace nvm
{
    namespace Interrupts
    {
        void print_char(const NVMMemory& memory)
        {
            __builtin_printf("%c", '?');
        }
    }
    
    constexpr Array<u64,
}