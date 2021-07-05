#include "Assembler.h"
#include "NVMBinaryFormat.h"
#include <IterableUtil.h>
#include <Preprocessor.h>
#include <StringView.h>
#include <Tuple.h>
#include <stdio.h>

#define VERSION STRINGIFY(0.1)

/*
 * NanoVM - A small vm to play with
 * Instructions can be 32 bit or 64 bit wide. The long instructions usually contain an immediate value.
 * It has 8 64s-bit general purpose registers named r1 to r8.
 * The register r0 is always 0 and cannot be written to.
 * The register sp is a special register used to hold the stack pointer.
 * The register ip is a special register that holds the next instruction to execute. It cannot be written to directly.
 * The following instructions are supported:
 *     add (dest)reg, (op1)reg, (op2)reg/imm
 *     sub (dest)reg, (minuend)reg, (subtrahend)reg/imm
 *     mul (dest)reg, (op1)reg, (op2)reg/imm (if the result is larger than 64 bits, the high 64 bits are discarded)
 *     div (dest)reg, (dividend)reg, (divisor)reg/imm
 *     neg (dest)reg, (source)reg
 *     not (dest)reg, (source)reg
 *     shl (dest)reg, (what)reg, (by)reg/imm
 *     shr (dest)reg, (what)reg, (by)reg/imm
 *     sra (dest)reg, (what)reg, (by)reg/imm (arithmetic shift)
 *     and (dest)reg, (op1)reg, (op2)reg/imm
 *     or (dest)reg, (op1)reg, (op2)reg/imm
 *     xor (dest)reg, (op1)reg, (op2)reg/imm
 *     load [64/32/16/8] (from)[reg/imm/tag] to (to)reg
 *     store [64/32/16/8] (what)reg in (where)[reg/imm/tag]
 *     int (interrupt code)
 *     jmp (where)reg/imm_offset
 *     jmp (where)reg/imm_offset if (op1)reg == (op2)reg
 *     jmp (where)reg/imm_offset if (op1)reg != (op2)reg
 *     jmp (where)reg/imm_offset if (op1)reg > (op2)reg
 *     jmp (where)reg/imm_offset if (op1)reg > (op2)reg unsigned
 *     jmp (where)reg/imm_offset if (op1)reg < (op2)reg
 *     jmp (where)reg/imm_offset if (op1)reg < (op2)reg unsigned
 *
 * The following describes the instruction format:
 *     Archetype 1:
 *     A|B|CCCCCC|DDDD|EEEE|FFFF|GGGGGGGGGGGG[GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG]
 *     A: 1 if instruction is 64 bits wide, otherwise 0.
 *     B: 1 if register field (F) for third operand is used, otherwise 0.
 *     C: Instruction opcode
 *     D: First register field
 *     E: Second register field
 *     F: Third register field
 *     G: Immediate field (12 bytes if <4096, otherwise 44 bits)
 *
 * The assembler accepts the following directives:
 *     .addr <address>
 *     .i8 <value>
 *     .i16 <value>
 *     .i32 <value>
 *     .i64 <value>
 *     .string <value>
 *
 *
 *
 *    Runtime:
 *    Interrupt table:
 *    0xFF - terminate execution returning error code in r1
 *    0x00 - read char from stdio. char stored in r1
 *    0x01 - read 64 bit integer from stdio. integer stored in r1
 *    0x02 - read string from stdio. null terminator included. string stored in memory starting in the memory pointed by r1
 *    0x03 - print char to stdio. char read from r1
 *    0x04 - print 64 bit integer to stdio. integer read from r1
 *    0x05 - print a null terminated string. string read from the memory pointed by r1
 *    0x30 - print a utf8 character to stdio. char read from r1
 *    0x32 - prints a newline to stdio
 *
 *
 *
 *   NVM uses a simple executable format that defines the entry point and memory load offset of the binary.
 *   The following table defines this format:
 *   AAAAAAAA|BBBBBBBB|CCCCCCCCCCCCCCCC|DDDDDDDDDDDDDDDD|X...
 *   A: magic signature (0x63026302)
 *   B: crc32 checksum of the file (excluding magic and this field)
 *   C: binary load memory offset
 *   D: entry point address
 *   X: binary payload
 *
 *
 */

void help()
{
    printf(
        "\nNanoVM - v" VERSION " by \e[32mngc6302h\e[39m\n"
        "This program is released under GPL3 license.\n"
        "Please check LICENSE.txt for a copy of the license.\n\n"
        "Usage:\n"
        "    \e[1m nvm <assembly code file> \e[0m\n\n"
        "\e[4mPress ctrl+c at any time to stop execution.\e[0m\n");
}

void error(const char* message)
{
    printf("\n\e[31mError: %s\e[0m", message);
}

const char* register_string(nvm::Register i)
{
    return find(nvm::register_literals, i, [](const Pair<StringView, nvm::Register>& a, const nvm::Register& b) -> bool
        { return a.get<nvm::Register>() == b; })
        ->get<StringView>()
        .non_null_terminated_buffer();
}

int main(int argc, char** argv)
{
    Vector<i8> k;
    if (argc < 2)
    {
        error("Insufficient argument count!\n\n");
        help();
        return -1;
    }
    printf("\nNanoVM - v" VERSION " by ngc6302h\n");

    auto maybe_assembler = nvm::Assembler::create_from_file(argv[1]);
    if (!maybe_assembler.has_value())
    {
        error("Couldn't use the specified file!\n");
        return -1;
    }
    auto assembler = move(maybe_assembler.value());
    auto tokens_or_errors = assembler.tokenize();
    if (tokens_or_errors.has_result())
    {
        for (const auto& tok : tokens_or_errors.result())
        {
            printf("At: L%zu P%zu Type: %s Value:%s\n", tok.position_in_source.line, tok.position_in_source.pos, tok.type == nvm::TokenType::NumericLiteral ? "NumericLiteral" : tok.type == nvm::TokenType::StringLiteral ? "StringLiteral"
                    : tok.type == nvm::TokenType::RegisterKeyword                                                                                                                                                          ? "RegisterKeyword"
                    : tok.type == nvm::TokenType::InstructionKeyword                                                                                                                                                       ? "InstructionKeyword"
                    : tok.type == nvm::TokenType::AssemblerDirective                                                                                                                                                       ? "AssemblerDirective"
                    : tok.type == nvm::TokenType::TagDefinition                                                                                                                                                            ? "TagDefinition"
                                                                                                                                                                                                                           : "Tag",
                tok.data->null_terminated_characters());
        }

        printf("\n\nObjects:\n");
        auto objects_or_errors = assembler.parse(tokens_or_errors.result());

        if (objects_or_errors.has_result())
        {
            for (const auto& obj : objects_or_errors.result())
            {
                if (obj.type == nvm::ObjectType::Instruction)
                {
                    const auto& data = reinterpret_cast<nvm::InstructionData*>(obj.data);
                    switch (data->instruction)
                    {
                    case nvm::Instruction::Add:
                    case nvm::Instruction::Sub:
                    case nvm::Instruction::Mul:
                    case nvm::Instruction::Div:
                    case nvm::Instruction::Neg:
                    case nvm::Instruction::Not:
                    case nvm::Instruction::Shl:
                    case nvm::Instruction::Shr:
                    case nvm::Instruction::Sra:
                    case nvm::Instruction::And:
                    case nvm::Instruction::Or:
                    case nvm::Instruction::Xor:
                    {
                        printf("Instruction: %s op1: %s op2: %s ",
                            find(nvm::instruction_literals, data->instruction,
                                [](const Pair<StringView, nvm::Instruction>& a,
                                    const nvm::Instruction& b) -> bool
                                { return a.get<nvm::Instruction>() == b; })
                                ->get<StringView>()
                                .non_null_terminated_buffer(),
                            register_string(data->op1),
                            register_string(data->op2));
                        if (data->op3.get<0>() == 0)
                            printf("op3: %s\n", register_string(data->op3.get<1>().get<nvm::Register>()));
                        else
                            printf("op3: %zu\n", data->op3.get<1>().get<u64>());
                    }
                    break;
                    case nvm::Instruction::Load:
                    {
                        char buff[10] { 0 };
                        if (data->op3.get<0>() == 2)
                        {
                            snprintf(buff, 10, "%zu", data->op3.get<1>().get<u64>());
                        }
                        printf("Instruction: load %lu from: %s to: %s\n",
                            data->misc, data->op3.get<0>() == 0 ? register_string(data->op2) : data->op3.get<int>() == 1 ? data->op3.get<1>().get<String>().null_terminated_characters()
                                                                                                                         : buff,
                            register_string(data->op1));
                    }
                    break;
                    case nvm::Instruction::Store:
                    {
                        char buff[10] { 0 };
                        if (data->op3.get<0>() == 2)
                        {
                            snprintf(buff, 10, "%zu", data->op3.get<1>().get<u64>());
                        }
                        printf("Instruction: store %lu what: %s in: %s\n", data->misc, register_string(data->op1),
                            data->op3.get<0>() == 0       ? register_string(data->op2)
                                : data->op3.get<0>() == 1 ? data->op3.get<1>().get<String>().null_terminated_characters()
                                                          : buff);
                    }
                    break;
                    case nvm::Instruction::Jmp:
                    case nvm::Instruction::Je:
                    case nvm::Instruction::Jne:
                    case nvm::Instruction::Jl:
                    case nvm::Instruction::Jlu:
                    case nvm::Instruction::Jg:
                    case nvm::Instruction::Jgu:
                    {
                        char buff[20] { 0 };
                        if (data->op3.get<0>() == 2)
                        {
                            snprintf(buff, 19, "%zu", data->op3.get<1>().get<u64>());
                        }
                        const char* operator_literal;
                        switch (data->instruction)
                        {
                            case nvm::Instruction::Je:
                                operator_literal = "==";
                                break;
                            case nvm::Instruction ::Jne:
                                operator_literal = "!=";
                                break;
                            case nvm::Instruction ::Jl:
                            case nvm::Instruction::Jlu:
                                operator_literal = "<";
                                break;
                            case nvm::Instruction::Jg:
                            case nvm::Instruction::Jgu:
                                operator_literal = ">";
                                break;
                            default: break;
                        }
                        printf("Instruction: jmp %s if %s %s %s\n", data->op3.get<0>() == 0 ? register_string(data->op3.get<1>().get<nvm::Register>()) :  data->op3.get<0>() == 1 ?
                                data->op3.get<1>().get<String>().null_terminated_characters() : buff, register_string(data->op1), operator_literal ,register_string(data->op2));
                    }
                    break;
                    case nvm::Instruction::Int:
                    {
                        printf("Instruction: int %lx\n", data->op3.get<1>().get<u64>());
                    }
                    break;
                    }
                }
            }
            
            auto bytecode_or_error = assembler.generate_bytecode(objects_or_errors.result());
            if (bytecode_or_error.has_result())
            {
                printf("Bytecode:\n");
                for (size_t i = 0; i < bytecode_or_error.result().size(); i+=4)
                {
                    printf("%02x%02x%02x%02x\n", bytecode_or_error.result()[i+3], bytecode_or_error.result()[i+2], bytecode_or_error.result()[i+1], bytecode_or_error.result()[i]);
                }
                
                auto bytecode = bytecode_or_error.result().span().as<u32>(); //this only works for little endian machines!!!1
                
            }
            else
            {
                for(const auto& e : bytecode_or_error.error())
                    printf("Error: %s", e.what->null_terminated_characters());
            }
        }
        else
        {
            for (const auto& err : objects_or_errors.error())
            {
                printf("At: L%zu P%zu What: %s\n", err.where.line, err.where.pos, err.what->null_terminated_characters());
            }
        }
    }
    return 0;
}
