#pragma once
#include <Array.h>
#include <Tuple.h>
#include <String.h>
#include <ResultOrError.h>
#include <Variant.h>
#include "Util.h"

namespace nvm
{
    enum class TokenType
    {
        InstructionKeyword,
        RegisterKeyword,
        NumericLiteral,
        StringLiteral,
        TagDefinition,
        Tag,
        AssemblerDirective,
        OtherKeyword
    };
    
    enum class ObjectType
    {
        Instruction,
        Tag,
        AssemblerDirective
    };
    
    enum class Directive
    {
        addr,
        i8,
        i16,
        i32,
        i64,
        string
    };
    
    enum class Instruction
    {
        Add = 0,
        Sub,
        Mul,
        Div,
        Neg,
        Not,
        Shl,
        Shr,
        Sra,
        And,
        Or,
        Xor,
        Load,
        Store,
        Int,
        Jmp,
        Je,
        Jne,
        Jg,
        Jgu,
        Jl,
        Jlu
    };
    
    enum class Register
    {
        r0 = 0,
        r1,
        r2,
        r3,
        r4,
        r5,
        r6,
        r7,
        r8,
        sp,
        ip
    };
    
    struct Error
    {
        LinePos where;
        String* what;
    };
    
    struct ResolvedTag
    {
        u64 address;
    };
    
    struct DirectiveData
    {
        Directive directive;
        Variant<String, u64> value;
    };
    
    struct InstructionData
    {
        Instruction instruction;
        Register op1;
        Register op2;
        Pair<int, Variant<Register, String, u64>> op3;
        u64 misc;
    };
    
    struct Object
    {
        ObjectType type;
        void* data;
    };
    
    struct Token
    {
        LinePos position_in_source;
        TokenType type;
        String* data;
    };
    
    struct InstructionParser
    {
        StringView instruction_literal;
        ResultOrError<Object, Error>(*parse)(Vector<const Token>::BidIt&, const Vector<const Token>::BidIt&);
        
        constexpr bool operator!=(const InstructionParser& other) const
        {
            return instruction_literal != other.instruction_literal;
        }
    };
    
    struct DirectiveParser
    {
        StringView directive;
        ResultOrError<Object, Error>(*parse)(Vector<const Token>::BidIt&, const Vector<const Token>::BidIt&, bool& more);
    };
    
    constexpr Array<Pair<StringView, Instruction>, 15> instruction_literals { { { "add", Instruction::Add }, { "sub", Instruction::Add }, { "mul", Instruction::Mul }, { "div", Instruction::Div }, { "neg", Instruction::Neg }, { "not", Instruction::Not }, { "shl", Instruction::Shl }, { "shr", Instruction::Shr }, { "sra", Instruction::Sra }, { "and", Instruction::And }, { "or", Instruction::Or }, { "xor", Instruction::Xor }, { "load", Instruction::Load }, {"store", Instruction::Store }, {"jmp", Instruction::Jmp } } };
    
    constexpr Array<Pair<StringView, Directive>, 6> assembler_directives { { { ".addr", Directive::addr }, { ".i8", Directive::i8 }, { ".i16", Directive::i16 }, { ".i32", Directive::i32 }, { ".i64", Directive::i64 }, { ".string", Directive::string } } };
    
    constexpr Array<Pair<StringView, Register>, 11> register_literals { { { "r0", Register::r0 }, { "r1", Register::r1 }, { "r2", Register::r2 }, { "r3", Register::r3 }, { "r4", Register::r4 }, { "r5", Register::r5 }, { "r6", Register::r6 }, { "r7", Register::r7 }, { "r8", Register::r8 }, { "sp", Register::sp }, { "ip", Register::ip } } };
    
    constexpr Array<StringView, 7> other_keyword_literals { { "to", "in", "if", "==", "!=", ">", "<" } };
    
    constexpr u8 get_instruction_opcode(Instruction i)
    {
        return (u8)i;
    }
    
    constexpr u8 get_register_id(Register r)
    {
        return (u8)r;
    }
    
    constexpr bool is_logicarithmetic(Instruction i)
    {
        switch (i)
        {
            case Instruction::Add:
            case Instruction::Sub:
            case Instruction::Mul:
            case Instruction::Div:
            case Instruction::Neg:
            case Instruction::Not:
            case Instruction::Shl:
            case Instruction::Shr:
            case Instruction::Sra:
            case Instruction::And:
            case Instruction::Or:
            case Instruction::Xor:
                return true;
            default:
                return false;
        }
    }
    
    constexpr bool is_load_store(Instruction i)
    {
        if (i == Instruction::Load || i == Instruction::Store)
            return true;
        return false;
    }
    
    constexpr bool is_interrupt(Instruction i)
    {
        if (i == Instruction::Load || i == Instruction::Store)
            return true;
        return false;
    }
    
    
    constexpr bool is_jump(Instruction i)
    {
        switch (i)
        {
            case Instruction::Jmp:
            case Instruction::Je:
            case Instruction::Jne:
            case Instruction::Jg:
            case Instruction::Jgu:
            case Instruction::Jl:
            case Instruction::Jlu:
                return true;
            default:
                return false;
        }
    }
}