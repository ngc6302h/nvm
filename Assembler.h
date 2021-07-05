#pragma once
#include <String.h>
#include <Vector.h>
#include <Optional.h>
#include <SmartPtr.h>
#include <Error.h>
#include <Variant.h>
#include <ResultOrError.h>
#include "Util.h"
#include "NVMData.h"
#include <stdio.h>

namespace nvm
{
    class Assembler
    {
    private:
    
    public:
        static Optional<Assembler> create_from_file(const StringView &path);
        
        void pretty_print_source()
        {
            //TODO
        }
        
        ResultOrError<Vector<Token>, Vector<Error>> tokenize();
        ResultOrError<Vector<Object>, Vector<Error>> parse(const Vector<Token>& tokens);
        ResultOrError<RefPtr<Vector<u8>>, Vector<Error>> generate_bytecode(const Vector<Object> &objects);
    
    private:
        explicit Assembler(Vector<u8>&&data);
        
        Vector<u8> m_data;
        StringView m_source;
    };
    
}
