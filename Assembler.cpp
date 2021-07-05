#include "Assembler.h"
#include "NVMData.h"
#include "NVMBinaryFormat.h"
#include <Array.h>
#include <File.h>
#include <Hashmap.h>
#include <IterableUtil.h>
#include <StringBuilder.h>
#include <Tuple.h>
#include <ctype.h>
#include <locale.h>
#include <stdlib.h>

namespace nvm
{
    ResultOrError<Tuple<Register, Register, bool, Variant<Register, u64>>, Error>
    read_reg_reg_regimm(Vector<const Token>::BidIt &token_iterator, const Vector<const Token>::BidIt &end,
                        const StringView &instruction_literal)
    {
        errno = 0;
        if (token_iterator->type != TokenType::RegisterKeyword)
        {
            return Error{
                    token_iterator->position_in_source, new String(
                            StringBuilder().append("invalid token in ").append(instruction_literal).append(
                                    " instruction (operand 1); expected register identifier").to_string())};
        }
        Register op1 = find(register_literals, token_iterator->data->to_view(),
                            [](const Pair<StringView, Register> &p, const StringView &s) -> bool
                            { return p.get<StringView>() == s; })
                ->get<Register>();
        
        if (++token_iterator == end)
            return Error{
                    (token_iterator--)->position_in_source,
                    new String("unexpected end of token stream in the middle of parsing an instruction")};
        if (token_iterator->type != TokenType::RegisterKeyword)
        {
            return Error{
                    token_iterator->position_in_source, new String(
                            StringBuilder().append("invalid token in ").append(instruction_literal).append(
                                    " instruction (operand 2); expected register identifier").to_string())};
        }
        Register op2 = find(register_literals, token_iterator->data->to_view(),
                            [](const Pair<StringView, Register> &p, const StringView &s) -> bool
                            { return p.get<StringView>() == s; })
                ->get<Register>();
        if (++token_iterator == end)
            return Error{
                    (token_iterator--)->position_in_source,
                    new String("unexpected end of token stream in the middle of parsing an instruction")};
        if (token_iterator->type == TokenType::RegisterKeyword)
        {
            Register op3 = find(register_literals, token_iterator->data->to_view(),
                                [](const Pair<StringView, Register> &p, const StringView &s) -> bool
                                { return p.get<StringView>() == s; })
                    ->get<Register>();
            return make_tuple<Register, Register, bool, Variant<Register, u64>>(op1, op2, true, op3);
        } else if (token_iterator->type == TokenType::NumericLiteral)
        {
            char *invalid_char;
            i64 op3;
            if (token_iterator->data->contains("x") || token_iterator->data->contains("X"))
                op3 = strtol(token_iterator->data->null_terminated_characters(), &invalid_char, 16);
            else
                op3 = strtol(token_iterator->data->null_terminated_characters(), &invalid_char, 10);
            if ((op3 & 0xFFFFF00000000000) != 0 || errno == ERANGE)
            {
                return Error{token_iterator->position_in_source, new String("overflow in register immediate operand")};
            }
            return make_tuple<Register, Register, bool, Variant<Register, u64>>(op1, op2, false, (u64) op3);
        } else
        {
            return Error{
                    token_iterator->position_in_source, new String(
                            StringBuilder().append("invalid token in ").append(instruction_literal).append(
                                    " instruction (operand 3); expected register identifier or numeric literal").to_string())};
        }
    }
    
    ResultOrError<Tuple<Register, Register>, Error>
    read_reg_reg(Vector<const Token>::BidIt &token_iterator, const Vector<const Token>::BidIt &end,
                 const StringView &instruction_literal)
    {
        errno = 0;
        if (token_iterator->type != TokenType::RegisterKeyword)
        {
            return Error{
                    token_iterator->position_in_source, new String(
                            StringBuilder().append("invalid token in ").append(instruction_literal).append(
                                    " instruction (operand 1); expected register identifier").to_string())};
        }
        Register op1 = find(register_literals, token_iterator->data->to_view(),
                            [](const Pair<StringView, Register> &p, const StringView &s) -> bool
                            { return p.get<StringView>() == s; })
                ->get<Register>();
        
        if (++token_iterator == end)
            return Error{
                    (token_iterator--)->position_in_source,
                    new String("unexpected end of token stream in the middle of parsing an instruction")};
        if (token_iterator->type != TokenType::RegisterKeyword)
        {
            return Error{
                    token_iterator->position_in_source, new String(
                            StringBuilder().append("invalid token in ").append(instruction_literal).append(
                                    " instruction (operand 2); expected register identifier").to_string())};
        }
        Register op2 = find(register_literals, token_iterator->data->to_view(),
                            [](const Pair<StringView, Register> &p, const StringView &s) -> bool
                            { return p.get<StringView>() == s; })
                ->get<Register>();
        return make_tuple<Register, Register>(op1, op2);
    }
    
    ResultOrError<Tuple<int, Variant<Register, u64, String>, Register, Instruction, Register>, Error>
    read_regimm_reg_ins_reg(Vector<const Token>::BidIt &token_iterator, const Vector<const Token>::BidIt &end,
                            const StringView &instruction_literal)
    {
        errno = 0;
        Register op1reg;
        u64 op1num;
        String op1tag;
        bool has_op1reg = false;
        bool has_op1num = false;
        if (token_iterator->type == TokenType::RegisterKeyword)
        {
            has_op1reg = true;
            has_op1num = false;
            op1reg = find(register_literals, token_iterator->data->to_view(),
                          [](const Pair<StringView, Register> &p, const StringView &s) -> bool
                          { return p.get<StringView>() == s; })
                    ->get<Register>();
        } else if (token_iterator->type == TokenType::NumericLiteral)
        {
            char *invalid_char;
            if (token_iterator->data->contains("x") || token_iterator->data->contains("X"))
                op1num = strtol(token_iterator->data->null_terminated_characters(), &invalid_char, 16);
            else
                op1num = strtol(token_iterator->data->null_terminated_characters(), &invalid_char, 10);
            if ((op1num & 0xFFFFF00000000000) != 0 || errno == ERANGE)
            {
                return Error{token_iterator->position_in_source, new String("overflow in register immediate operand")};
            }
            has_op1reg = false;
            has_op1num = true;
        } else if (token_iterator->type == TokenType::Tag)
        {
            has_op1reg = false;
            has_op1num = false;
            op1tag = *token_iterator->data;
        } else
        {
            return Error{
                    token_iterator->position_in_source, new String(
                            StringBuilder().append("invalid token in ").append(instruction_literal).append(
                                    " instruction (operand 1); expected register identifier, tag or numeric literal").to_string())};
        }
        Variant<Register, u64, String> op1 = has_op1reg ? Variant<Register, u64, String>(op1reg) : has_op1num
                                                                                                   ? Variant<Register, u64, String>(
                        op1num)
                                                                                                   : Variant<Register, u64, String>(
                        op1tag);
        if (++token_iterator == end)
            return Error{
                    (token_iterator--)->position_in_source,
                    new String("unexpected end of token stream in the middle of parsing an instruction")};
        if (token_iterator->type != TokenType::OtherKeyword)
        {
            //assume this is an unconditional jmp
            return make_tuple<int, Variant<Register, u64, String>, Register, Instruction, Register>(
                    has_op1reg ? 0 : has_op1num ? 1
                                                : 2,
                    op1, Register::r0, Instruction::Je, Register::r0);
        }
        if (++token_iterator == end)
            return Error{
                    (token_iterator--)->position_in_source,
                    new String("unexpected end of token stream in the middle of parsing an instruction")};
        Register op2 = find(register_literals, token_iterator->data->to_view(),
                            [](const Pair<StringView, Register> &p, const StringView &s) -> bool
                            { return p.get<StringView>() == s; })
                ->get<Register>();
        if (++token_iterator == end)
            return Error{
                    (token_iterator--)->position_in_source,
                    new String("unexpected end of token stream in the middle of parsing an instruction")};
        Instruction ins;
        if (token_iterator->type != TokenType::OtherKeyword)
        {
            return Error{
                    token_iterator->position_in_source,
                    new String("unexpected keyword found while parsing a jmp-type instruction")};
        } else
        {
            if (token_iterator->data->to_view() == "<"_sv)
            {
                ins = Instruction::Jl;
            } else if (token_iterator->data->to_view() == ">"_sv)
            {
                ins = Instruction::Jg;
            } else if (token_iterator->data->to_view() == "=="_sv)
            {
                ins = Instruction::Je;
            } else if (token_iterator->data->to_view() == "!="_sv)
            {
                ins = Instruction::Jne;
            } else
            {
                return Error{
                        token_iterator->position_in_source,
                        new String("unexpected keyword found while parsing a jmp-type instruction")};
            }
        }
        if (++token_iterator == end)
            return Error{
                    (token_iterator--)->position_in_source,
                    new String("unexpected end of token stream in the middle of parsing an instruction")};
        Register op3 = find(register_literals, token_iterator->data->to_view(),
                            [](const Pair<StringView, Register> &p, const StringView &s) -> bool
                            { return p.get<StringView>() == s; })
                ->get<Register>();
        auto next = token_iterator;
        if (++next != end)
        {
            if (next->type == TokenType::OtherKeyword && next->data->to_view() == "unsigned")
            {
                if (ins == Instruction::Jg)
                    ins = Instruction::Jgu;
                else if (ins == Instruction::Jl)
                    ins = Instruction::Jlu;
                else
                    return Error{
                            token_iterator->position_in_source,
                            new String("unsigned keyword can only be used with '<' and '>' jmp types")
                    };
            }
        }
        return make_tuple<int, Variant<Register, u64, String>, Register, Instruction, Register>(
                has_op1reg ? 0 : has_op1num ? 1
                                            : 2,
                op1, op2, ins, op3);
    }
    
    constexpr Array<InstructionParser, 16> instruction_parsers{
            {{
                     "add",
                     [](Vector<const Token>::BidIt &token_iterator,
                        const Vector<const Token>::BidIt &end) -> ResultOrError<Object, Error>
                     {
                         auto result_or_error = read_reg_reg_regimm(token_iterator, end, "add");
                         if (result_or_error.has_error())
                             return result_or_error.error();
                        
                         auto[op1, op2, op3_is_register, op3] = result_or_error.result();
                         if (op3_is_register)
                             return Object{
                                     ObjectType::Instruction,
                                     new InstructionData{
                                             Instruction::Add, op1, op2, make_tuple(0, Variant<Register, String, u64>(
                                                     op3.get<Register>())), 0}
                             };
                         else
                             return Object{
                                     ObjectType::Instruction,
                                     new InstructionData{
                                             Instruction::Add, op1, op2,
                                             make_tuple(2, Variant<Register, String, u64>(op3.get<u64>())), 2}
                             };
                     }},
                    {
                            "sub",
                            [](Vector<const Token>::BidIt &token_iterator,
                               const Vector<const Token>::BidIt &end) -> ResultOrError<Object, Error>
                            {
                                auto result_or_error = read_reg_reg_regimm(token_iterator, end, "sub");
                                if (result_or_error.has_error())
                                    return result_or_error.error();
                                
                                auto[op1, op2, op3_is_register, op3] = result_or_error.result();
                                if (op3_is_register)
                                    return Object{
                                            ObjectType::Instruction,
                                            new InstructionData{
                                                    Instruction::Sub, op1, op2, make_tuple(0,
                                                                                           Variant<Register, String, u64>(
                                                                                                   op3.get<Register>())),
                                                    0}
                                    };
                                else
                                    return Object{
                                            ObjectType::Instruction,
                                            new InstructionData{
                                                    Instruction::Sub, op1, op2, make_tuple(2,
                                                                                           Variant<Register, String, u64>(
                                                                                                   op3.get<u64>())), 2}
                                    };
                            }},
                    {
                            "mul",
                            [](Vector<const Token>::BidIt &token_iterator,
                               const Vector<const Token>::BidIt &end) -> ResultOrError<Object, Error>
                            {
                                auto result_or_error = read_reg_reg_regimm(token_iterator, end, "mul");
                                if (result_or_error.has_error())
                                    return result_or_error.error();
                                
                                auto[op1, op2, op3_is_register, op3] = result_or_error.result();
                                if (op3_is_register)
                                    return Object{
                                            ObjectType::Instruction,
                                            new InstructionData{
                                                    Instruction::Mul, op1, op2, make_tuple(0,
                                                                                           Variant<Register, String, u64>(
                                                                                                   op3.get<Register>())),
                                                    0}
                                    };
                                else
                                    return Object{
                                            ObjectType::Instruction,
                                            new InstructionData{
                                                    Instruction::Mul, op1, op2, make_tuple(2,
                                                                                           Variant<Register, String, u64>(
                                                                                                   op3.get<u64>())), 2}
                                    };
                            }},
                    {
                            "div",
                            [](Vector<const Token>::BidIt &token_iterator,
                               const Vector<const Token>::BidIt &end) -> ResultOrError<Object, Error>
                            {
                                auto result_or_error = read_reg_reg_regimm(token_iterator, end, "div");
                                if (result_or_error.has_error())
                                    return result_or_error.error();
                                
                                auto[op1, op2, op3_is_register, op3] = result_or_error.result();
                                if (op3_is_register)
                                    return Object{
                                            ObjectType::Instruction,
                                            new InstructionData{
                                                    Instruction::Div, op1, op2, make_tuple(0,
                                                                                           Variant<Register, String, u64>(
                                                                                                   op3.get<Register>())),
                                                    0}
                                    };
                                else
                                    return Object{
                                            ObjectType::Instruction,
                                            new InstructionData{
                                                    Instruction::Div, op1, op2, make_tuple(2,
                                                                                           Variant<Register, String, u64>(
                                                                                                   op3.get<u64>())), 2}
                                    };
                            }},
                    {
                            "neg",
                            [](Vector<const Token>::BidIt &token_iterator,
                               const Vector<const Token>::BidIt &end) -> ResultOrError<Object, Error>
                            {
                                auto result_or_error = read_reg_reg(token_iterator, end, "neg");
                                if (result_or_error.has_error())
                                    return result_or_error.error();
                                
                                auto[op1, op2] = result_or_error.result();
                                return Object{
                                        ObjectType::Instruction, new InstructionData{
                                                Instruction::Neg, op1, op2,
                                                make_tuple(2, Variant<Register, String, u64>(0ul)), 0}
                                };
                            }},
                    {
                            "not",
                            [](Vector<const Token>::BidIt &token_iterator,
                               const Vector<const Token>::BidIt &end) -> ResultOrError<Object, Error>
                            {
                                auto result_or_error = read_reg_reg(token_iterator, end, "not");
                                if (result_or_error.has_error())
                                    return result_or_error.error();
                                
                                auto[op1, op2] = result_or_error.result();
                                return Object{
                                        ObjectType::Instruction, new InstructionData{
                                                Instruction::Neg, op1, op2,
                                                make_tuple(0, Variant<Register, String, u64>(0ul)), 0}
                                };
                            }},
                    {
                            "shl",
                            [](Vector<const Token>::BidIt &token_iterator,
                               const Vector<const Token>::BidIt &end) -> ResultOrError<Object, Error>
                            {
                                auto result_or_error = read_reg_reg_regimm(token_iterator, end, "shl");
                                if (result_or_error.has_error())
                                    return result_or_error.error();
                                
                                auto[op1, op2, op3_is_register, op3] = result_or_error.result();
                                if (op3_is_register)
                                    return Object{
                                            ObjectType::Instruction,
                                            new InstructionData{
                                                    Instruction::Shl, op1, op2, make_tuple(0,
                                                                                           Variant<Register, String, u64>(
                                                                                                   op3.get<Register>())),
                                                    0}
                                    };
                                else
                                    return Object{
                                            ObjectType::Instruction,
                                            new InstructionData{
                                                    Instruction::Shl, op1, op2, make_tuple(2,
                                                                                           Variant<Register, String, u64>(
                                                                                                   op3.get<u64>())), 2}
                                    };
                            }},
                    {
                            "shr",
                            [](Vector<const Token>::BidIt &token_iterator,
                               const Vector<const Token>::BidIt &end) -> ResultOrError<Object, Error>
                            {
                                auto result_or_error = read_reg_reg_regimm(token_iterator, end, "shr");
                                if (result_or_error.has_error())
                                    return result_or_error.error();
                                
                                auto[op1, op2, op3_is_register, op3] = result_or_error.result();
                                if (op3_is_register)
                                    return Object{
                                            ObjectType::Instruction,
                                            new InstructionData{
                                                    Instruction::Shr, op1, op2, make_tuple(0,
                                                                                           Variant<Register, String, u64>(
                                                                                                   op3.get<Register>())),
                                                    0}
                                    };
                                else
                                    return Object{
                                            ObjectType::Instruction,
                                            new InstructionData{
                                                    Instruction::Shr, op1, op2, make_tuple(2,
                                                                                           Variant<Register, String, u64>(
                                                                                                   op3.get<u64>())), 2}
                                    };
                            }},
                    {
                            "sra",
                            [](Vector<const Token>::BidIt &token_iterator,
                               const Vector<const Token>::BidIt &end) -> ResultOrError<Object, Error>
                            {
                                auto result_or_error = read_reg_reg_regimm(token_iterator, end, "sra");
                                if (result_or_error.has_error())
                                    return result_or_error.error();
                                
                                auto[op1, op2, op3_is_register, op3] = result_or_error.result();
                                if (op3_is_register)
                                    return Object{
                                            ObjectType::Instruction,
                                            new InstructionData{
                                                    Instruction::Sra, op1, op2, make_tuple(0,
                                                                                           Variant<Register, String, u64>(
                                                                                                   op3.get<Register>())),
                                                    0}
                                    };
                                else
                                    return Object{
                                            ObjectType::Instruction,
                                            new InstructionData{
                                                    Instruction::Sra, op1, op2, make_tuple(2,
                                                                                           Variant<Register, String, u64>(
                                                                                                   op3.get<u64>())), 2}
                                    };
                            }},
                    {
                            "and",
                            [](Vector<const Token>::BidIt &token_iterator,
                               const Vector<const Token>::BidIt &end) -> ResultOrError<Object, Error>
                            {
                                auto result_or_error = read_reg_reg_regimm(token_iterator, end, "and");
                                if (result_or_error.has_error())
                                    return result_or_error.error();
                                
                                auto[op1, op2, op3_is_register, op3] = result_or_error.result();
                                if (op3_is_register)
                                    return Object{
                                            ObjectType::Instruction,
                                            new InstructionData{
                                                    Instruction::And, op1, op2, make_tuple(0,
                                                                                           Variant<Register, String, u64>(
                                                                                                   op3.get<Register>())),
                                                    0}
                                    };
                                else
                                    return Object{
                                            ObjectType::Instruction,
                                            new InstructionData{
                                                    Instruction::And, op1, op2, make_tuple(2,
                                                                                           Variant<Register, String, u64>(
                                                                                                   op3.get<u64>())), 2}
                                    };
                            }},
                    {
                            "or",
                            [](Vector<const Token>::BidIt &token_iterator,
                               const Vector<const Token>::BidIt &end) -> ResultOrError<Object, Error>
                            {
                                auto result_or_error = read_reg_reg_regimm(token_iterator, end, "or");
                                if (result_or_error.has_error())
                                    return result_or_error.error();
                                
                                auto[op1, op2, op3_is_register, op3] = result_or_error.result();
                                if (op3_is_register)
                                    return Object{
                                            ObjectType::Instruction,
                                            new InstructionData{
                                                    Instruction::Or, op1, op2, make_tuple(0,
                                                                                          Variant<Register, String, u64>(
                                                                                                  op3.get<Register>())),
                                                    0}
                                    };
                                else
                                    return Object{
                                            ObjectType::Instruction,
                                            new InstructionData{
                                                    Instruction::Or, op1, op2, make_tuple(2,
                                                                                          Variant<Register, String, u64>(
                                                                                                  op3.get<u64>())), 2}
                                    };
                            }},
                    {
                            "xor",
                            [](Vector<const Token>::BidIt &token_iterator,
                               const Vector<const Token>::BidIt &end) -> ResultOrError<Object, Error>
                            {
                                auto result_or_error = read_reg_reg_regimm(token_iterator, end, "xor");
                                if (result_or_error.has_error())
                                    return result_or_error.error();
                                
                                auto[op1, op2, op3_is_register, op3] = result_or_error.result();
                                if (op3_is_register)
                                    return Object{
                                            ObjectType::Instruction,
                                            new InstructionData{
                                                    Instruction::Xor, op1, op2, make_tuple(0,
                                                                                           Variant<Register, String, u64>(
                                                                                                   op3.get<Register>())),
                                                    0}
                                    };
                                else
                                    return Object{
                                            ObjectType::Instruction,
                                            new InstructionData{
                                                    Instruction::Xor, op1, op2, make_tuple(2,
                                                                                           Variant<Register, String, u64>(
                                                                                                   op3.get<u64>())), 0}
                                    };
                            }},
                    {
                            "load",
                            [](Vector<const Token>::BidIt &token_iterator,
                               const Vector<const Token>::BidIt &end) -> ResultOrError<Object, Error>
                            {
                                errno = 0;
                                u64 op1;
                                if (token_iterator->type == TokenType::NumericLiteral)
                                {
                                    char *invalid_char;
                                    if (token_iterator->data->contains("x") || token_iterator->data->contains("X"))
                                        op1 = strtol(token_iterator->data->null_terminated_characters(), &invalid_char,
                                                     16);
                                    else
                                        op1 = strtol(token_iterator->data->null_terminated_characters(), &invalid_char,
                                                     10);
                                    if ((op1 & 0xFFFFF00000000000) != 0 || errno == ERANGE)
                                    {
                                        return Error{
                                                token_iterator->position_in_source,
                                                new String("overflow in register immediate operand")
                                        };
                                    }
                                } else
                                    return Error{
                                            token_iterator->position_in_source, new String(
                                                    "unexpected token found while parsing a instruction: expected numeric literal (64/32/16/8)")
                                    };
                                if (++token_iterator == end)
                                    return Error{
                                            (token_iterator--)->position_in_source, new String(
                                                    "unexpected end of token stream in the middle of parsing an instruction")
                                    };
                                Register op2reg;
                                u64 op2num;
                                String op2tag;
                                bool has_op2reg = false;
                                bool has_op2num = false;
                                if (token_iterator->type == TokenType::RegisterKeyword)
                                {
                                    has_op2reg = true;
                                    op2reg = find(register_literals, token_iterator->data->to_view(),
                                                  [](const Pair<StringView, Register> &p, const StringView &s) -> bool
                                                  { return p.get<StringView>() == s; })
                                            ->get<Register>();
                                } else if (token_iterator->type == TokenType::NumericLiteral)
                                {
                                    has_op2num = true;
                                    char *invalid_char;
                                    if (token_iterator->data->contains("x") || token_iterator->data->contains("X"))
                                        op2num = strtol(token_iterator->data->null_terminated_characters(),
                                                        &invalid_char, 16);
                                    else
                                        op2num = strtol(token_iterator->data->null_terminated_characters(),
                                                        &invalid_char, 10);
                                    if (op1 != 64 && op1 != 32 && op1 != 16 && op1 != 8)
                                        return Error{
                                                token_iterator->position_in_source, new String(
                                                        "load/store instructions can only move 64/32/16/8 bits at a time")
                                        };
                                } else if (token_iterator->type == TokenType::Tag)
                                {
                                    op2tag = *token_iterator->data;
                                } else
                                {
                                    return Error{
                                            token_iterator->position_in_source, new String(
                                                    "invalid token in load instruction (operand 2); expected register identifier or numeric literal")
                                    };
                                }
                                if (++token_iterator == end)
                                    return Error{
                                            (token_iterator--)->position_in_source, new String(
                                                    "unexpected end of token stream in the middle of parsing an instruction")
                                    };
                                if (token_iterator->type != TokenType::OtherKeyword &&
                                    token_iterator->data->to_view() != "to")
                                {
                                    return Error{
                                            token_iterator->position_in_source,
                                            new String("unexpected keyword found while parsing a load instruction")
                                    };
                                }
                                if (++token_iterator == end)
                                    return Error{
                                            (token_iterator--)->position_in_source, new String(
                                                    "unexpected end of token stream in the middle of parsing an instruction")
                                    };
                                Register op3 = find(register_literals, token_iterator->data->to_view(),
                                                    [](const Pair<StringView, Register> &p, const StringView &s) -> bool
                                                    { return p.get<StringView>() == s; })
                                        ->get<Register>();
                                return Object{
                                        ObjectType::Instruction, new InstructionData{
                                                .instruction = Instruction::Load, .op1 = op3, .op3 = has_op2reg
                                                                                                     ? make_tuple(0,
                                                                                                                  Variant<Register, String, u64>(
                                                                                                                          op2reg))
                                                                                                     : has_op2num
                                                                                                       ? make_tuple(2,
                                                                                                                    Variant<Register, String, u64>(
                                                                                                                            op2num))
                                                                                                       : make_tuple(1,
                                                                                                                    Variant<Register, String, u64>(
                                                                                                                            op2tag)),
                                                .misc = op1}
                                };
                            }},
                    {
                            "store",
                            [](Vector<const Token>::BidIt &token_iterator,
                               const Vector<const Token>::BidIt &end) -> ResultOrError<Object, Error>
                            {
                                errno = 0;
                                u64 op1;
                                if (token_iterator->type == TokenType::NumericLiteral)
                                {
                                    char *invalid_char;
                                    if (token_iterator->data->contains("x") || token_iterator->data->contains("X"))
                                        op1 = strtol(token_iterator->data->null_terminated_characters(),
                                                     &invalid_char, 16);
                                    else
                                        op1 = strtol(token_iterator->data->null_terminated_characters(),
                                                     &invalid_char, 10);
                                    if (op1 != 64 && op1 != 32 && op1 != 16 && op1 != 8)
                                        return Error{
                                                token_iterator->position_in_source,
                                                new String(
                                                        "load/store instructions can only move 64/32/16/8 bits at a time")
                                        };
                                } else
                                    return Error{
                                            token_iterator->position_in_source, new String(
                                                    "unexpected token found while parsing a instruction: expected numeric literal (64/32/16/8)")
                                    };
                                if (++token_iterator == end)
                                    return Error{
                                            (token_iterator--)->position_in_source,
                                            new String(
                                                    "unexpected end of token stream in the middle of parsing an instruction")
                                    };
                                Register op2 = find(register_literals, token_iterator->data->to_view(),
                                                    [](const Pair<StringView, Register> &p,
                                                       const StringView &s) -> bool
                                                    { return p.get<StringView>() == s; })
                                        ->get<Register>();
                                if (++token_iterator == end)
                                    return Error{
                                            (token_iterator--)->position_in_source,
                                            new String(
                                                    "unexpected end of token stream in the middle of parsing an instruction")
                                    };
                                if (token_iterator->type != TokenType::OtherKeyword &&
                                    token_iterator->data->to_view() != "in")
                                {
                                    return Error{
                                            token_iterator->position_in_source,
                                            new String(
                                                    "unexpected keyword found while parsing a store instruction")
                                    };
                                }
                                if (++token_iterator == end)
                                    return Error{
                                            (token_iterator--)->position_in_source,
                                            new String(
                                                    "unexpected end of token stream in the middle of parsing an instruction")
                                    };
                                
                                Register op3reg;
                                u64 op3num;
                                String op3tag;
                                bool has_op3reg = false;
                                bool has_op3num = false;
                                if (token_iterator->type == TokenType::RegisterKeyword)
                                {
                                    has_op3reg = true;
                                    op3reg = find(register_literals, token_iterator->data->to_view(),
                                                  [](const Pair<StringView, Register> &p,
                                                     const StringView &s) -> bool
                                                  { return p.get<StringView>() == s; })
                                            ->get<Register>();
                                } else if (token_iterator->type == TokenType::NumericLiteral)
                                {
                                    has_op3num = true;
                                    char *invalid_char;
                                    if (token_iterator->data->contains("x") || token_iterator->data->contains("X"))
                                        op3num = strtol(token_iterator->data->null_terminated_characters(),
                                                        &invalid_char, 16);
                                    else
                                        op3num = strtol(token_iterator->data->null_terminated_characters(),
                                                        &invalid_char, 10);
                                    if ((op3num & 0xFFFFF00000000000) != 0 || errno == ERANGE)
                                    {
                                        return Error{
                                                token_iterator->position_in_source,
                                                new String("overflow in register immediate operand")
                                        };
                                    }
                                } else if (token_iterator->type == TokenType::Tag)
                                {
                                    op3tag = *token_iterator->data;
                                } else
                                {
                                    return Error{
                                            token_iterator->position_in_source, new String(
                                                    "invalid token in load instruction (operand 2); expected register identifier or numeric literal")
                                    };
                                }
                                if (++token_iterator == end)
                                    return Error{
                                            (token_iterator--)->position_in_source,
                                            new String(
                                                    "unexpected end of token stream in the middle of parsing an instruction")
                                    };
                                return Object{
                                        ObjectType::Instruction, new InstructionData{
                                                .instruction = Instruction::Store, .op1 = op2, .op3 = has_op3reg
                                                                                                      ? make_tuple(0,
                                                                                                                   Variant<Register, String, u64>(
                                                                                                                           op3reg))
                                                                                                      : has_op3num
                                                                                                        ? make_tuple(2,
                                                                                                                     Variant<Register, String, u64>(
                                                                                                                             op3num))
                                                                                                        : make_tuple(1,
                                                                                                                     Variant<Register, String, u64>(
                                                                                                                             op3tag)),
                                                .misc = op1}
                                };
                            }
                        
                    },
                    {
                            "int",
                            [](Vector<const Token>::BidIt &token_iterator,
                               const Vector<const Token>::BidIt &end) -> ResultOrError<Object, Error>
                            {
                                errno = 0;
                                if (token_iterator->type == TokenType::NumericLiteral)
                                {
                                    i64 op1;
                                    char *invalid_char;
                                    if (token_iterator->data->contains("x") || token_iterator->data->contains("X"))
                                        op1 = strtol(token_iterator->data->null_terminated_characters(),
                                                     &invalid_char, 16);
                                    else
                                        op1 = strtol(token_iterator->data->null_terminated_characters(),
                                                     &invalid_char, 10);
                                    if ((op1 & 0xFFFFF00000000000) != 0 || errno == ERANGE)
                                    {
                                        return Error{
                                                token_iterator->position_in_source,
                                                new String("overflow in register immediate operand")
                                        };
                                    }
                                    return Object{
                                            ObjectType::Instruction,
                                            new InstructionData{
                                                    .instruction = Instruction::Int, .op3 = make_tuple(2,
                                                                                                       Variant<Register, String, u64>(
                                                                                                               (u64) op1))}
                                    };
                                } else
                                {
                                    return Error{
                                            token_iterator->position_in_source, new String(
                                                    "invalid token in load instruction (operand 2); expected register identifier or numeric literal")
                                    };
                                }
                            }},
                    {
                            "jmp",
                            [](Vector<const Token>::BidIt &token_iterator,
                               const Vector<const Token>::BidIt &end) -> ResultOrError<Object, Error>
                            {
                                auto result_or_error = read_regimm_reg_ins_reg(token_iterator, end, "jmp");
                                if (result_or_error.has_error())
                                    return result_or_error.error();
                                
                                auto[op1_selector, op1, op2, ins, op3] = result_or_error.result();
                                if (op1_selector == 0)
                                    return Object{
                                            ObjectType::Instruction,
                                            new InstructionData{
                                                    ins, op2, op3, make_tuple(0, Variant<Register, String, u64>(
                                                            op1.get<Register>())), 0}
                                    };
                                else if (op1_selector == 1)
                                    return Object{
                                            ObjectType::Instruction,
                                            new InstructionData{
                                                    ins, op2, op3, make_tuple(2, Variant<Register, String, u64>(
                                                            op1.get<u64>())), 0}
                                    };
                                else
                                    return Object{
                                            ObjectType::Instruction,
                                            new InstructionData{
                                                    ins, op2, op3, make_tuple(1, Variant<Register, String, u64>(
                                                            op1.get<String>())), 0}
                                    };
                            }}}
    };
    
    constexpr Array<DirectiveParser, 6> directive_parsers{
            {{
                     ".i8",
                     [](Vector<const Token>::BidIt &token_iterator, const Vector<const Token>::BidIt &end,
                        bool &more) -> ResultOrError<Object, Error>
                     {
                         errno = 0;
                         if (token_iterator->type != TokenType::NumericLiteral)
                         {
                             return Error{
                                     token_iterator->position_in_source, new String(
                                             "unexpected token while parsing .i8 directive; expected numeric literal")};
                         }
                         char *invalid_char;
                         i64 value;
                         if ((*token_iterator).data->contains("x") || (*token_iterator).data->contains("X"))
                             value = strtol((*token_iterator).data->null_terminated_characters(), &invalid_char, 16);
                         else
                             value = strtol((*token_iterator).data->null_terminated_characters(), &invalid_char, 10);
                         if (invalid_char == (*token_iterator).data->null_terminated_characters())
                         {
                             return Error{
                                     token_iterator->position_in_source, new String("couldn't parse numeric token")};
                         }
                         if ((value & 0xFFFFFFFFFFFFFF00) != 0 || errno == ERANGE)
                         {
                             return Error{
                                     token_iterator->position_in_source,
                                     new String("overflow in i8 directive literal")};
                         }
                         auto next = token_iterator;
                         next++;
                         if (next != end && next->type == TokenType::NumericLiteral)
                             more = true;
                         else
                             more = false;
                         return Object{ObjectType::AssemblerDirective, new DirectiveData{Directive::i8, (u64) value}};
                     }},
                    {
                            ".i16",
                            [](Vector<const Token>::BidIt &token_iterator, const Vector<const Token>::BidIt &end,
                               bool &more) -> ResultOrError<Object, Error>
                            {
                                errno = 0;
                                if (token_iterator->type != TokenType::NumericLiteral)
                                {
                                    return Error{
                                            token_iterator->position_in_source, new String(
                                                    "unexpected token while parsing .i16 directive; expected numeric literal")};
                                }
                                char *invalid_char;
                                i64 value;
                                if ((*token_iterator).data->contains("x") || (*token_iterator).data->contains("X"))
                                    value = strtol((*token_iterator).data->null_terminated_characters(), &invalid_char,
                                                   16);
                                else
                                    value = strtol((*token_iterator).data->null_terminated_characters(), &invalid_char,
                                                   10);
                                if (invalid_char == (*token_iterator).data->null_terminated_characters())
                                {
                                    return Error{
                                            token_iterator->position_in_source,
                                            new String("couldn't parse numeric token")};
                                }
                                if ((value & 0xFFFFFFFFFFFF0000) != 0 || errno == ERANGE)
                                {
                                    return Error{
                                            token_iterator->position_in_source,
                                            new String("overflow in i16 directive literal")};
                                }
                                auto next = token_iterator;
                                next++;
                                if (next != end && next->type == TokenType::NumericLiteral)
                                    more = true;
                                else
                                    more = false;
                                return Object{
                                        ObjectType::AssemblerDirective, new DirectiveData{Directive::i16, (u64) value}};
                            }},
                    {
                            ".i32",
                            [](Vector<const Token>::BidIt &token_iterator, const Vector<const Token>::BidIt &end,
                               bool &more) -> ResultOrError<Object, Error>
                            {
                                errno = 0;
                                if (token_iterator->type != TokenType::NumericLiteral)
                                {
                                    return Error{
                                            token_iterator->position_in_source, new String(
                                                    "unexpected token while parsing .i32 directive; expected numeric literal")};
                                }
                                char *invalid_char;
                                i64 value;
                                if ((*token_iterator).data->contains("x") || (*token_iterator).data->contains("X"))
                                    value = strtol((*token_iterator).data->null_terminated_characters(), &invalid_char,
                                                   16);
                                else
                                    value = strtol((*token_iterator).data->null_terminated_characters(), &invalid_char,
                                                   10);
                                if (invalid_char == (*token_iterator).data->null_terminated_characters())
                                {
                                    return Error{
                                            token_iterator->position_in_source,
                                            new String("couldn't parse numeric token")};
                                }
                                if ((value & 0xFFFFFFFF00000000) != 0 || errno == ERANGE)
                                {
                                    return Error{
                                            token_iterator->position_in_source,
                                            new String("overflow in i32 directive literal")};
                                }
                                auto next = token_iterator;
                                next++;
                                if (next != end && next->type == TokenType::NumericLiteral)
                                    more = true;
                                else
                                    more = false;
                                return Object{
                                        ObjectType::AssemblerDirective, new DirectiveData{Directive::i32, (u64) value}};
                            }},
                    {
                            ".i64",
                            [](Vector<const Token>::BidIt &token_iterator, const Vector<const Token>::BidIt &end,
                               bool &more) -> ResultOrError<Object, Error>
                            {
                                errno = 0;
                                if (token_iterator->type != TokenType::NumericLiteral)
                                {
                                    return Error{
                                            token_iterator->position_in_source, new String(
                                                    "unexpected token while parsing .i64 directive; expected numeric literal")};
                                }
                                char *invalid_char;
                                i64 value;
                                if ((*token_iterator).data->contains("x") || (*token_iterator).data->contains("X"))
                                    value = strtol((*token_iterator).data->null_terminated_characters(), &invalid_char,
                                                   16);
                                else
                                    value = strtol((*token_iterator).data->null_terminated_characters(), &invalid_char,
                                                   10);
                                if (invalid_char == (*token_iterator).data->null_terminated_characters())
                                {
                                    return Error{
                                            token_iterator->position_in_source,
                                            new String("couldn't parse numeric token")};
                                }
                                if ((value & 0xFFFFFFFF00000000) != 0 || errno == ERANGE)
                                {
                                    return Error{
                                            token_iterator->position_in_source,
                                            new String("overflow in i64 directive literal")};
                                }
                                auto next = token_iterator;
                                next++;
                                if (next != end && next->type == TokenType::NumericLiteral)
                                    more = true;
                                else
                                    more = false;
                                return Object{
                                        ObjectType::AssemblerDirective, new DirectiveData{Directive::i64, (u64) value}};
                            }},
                    {
                            ".addr",
                            [](Vector<const Token>::BidIt &token_iterator, const Vector<const Token>::BidIt &end,
                               bool &more) -> ResultOrError<Object, Error>
                            {
                                errno = 0;
                                if (token_iterator->type != TokenType::NumericLiteral)
                                {
                                    return Error{
                                            token_iterator->position_in_source, new String(
                                                    "unexpected token while parsing .addr directive; expected numeric literal")};
                                }
                                char *invalid_char;
                                i64 value;
                                if ((*token_iterator).data->contains("x") || (*token_iterator).data->contains("X"))
                                    value = strtol((*token_iterator).data->null_terminated_characters(), &invalid_char,
                                                   16);
                                else
                                    value = strtol((*token_iterator).data->null_terminated_characters(), &invalid_char,
                                                   10);
                                if (invalid_char == (*token_iterator).data->null_terminated_characters())
                                {
                                    return Error{
                                            token_iterator->position_in_source,
                                            new String("couldn't parse numeric token")};
                                }
                                if ((value & 0xFFFFFFFF00000000) != 0 || errno == ERANGE)
                                {
                                    return Error{
                                            token_iterator->position_in_source,
                                            new String("overflow in addr directive literal")};
                                }
                                auto next = token_iterator;
                                next++;
                                if (next != end && next->type == TokenType::NumericLiteral)
                                    more = true;
                                else
                                    more = false;
                                return Object{
                                        ObjectType::AssemblerDirective,
                                        new DirectiveData{Directive::addr, (u64) value}};
                            }},
                    {
                            ".string",
                            [](Vector<const Token>::BidIt &token_iterator, const Vector<const Token>::BidIt &end,
                               bool &more) -> ResultOrError<Object, Error>
                            {
                                errno = 0;
                                if (token_iterator->type != TokenType::StringLiteral)
                                {
                                    return Error{
                                            token_iterator->position_in_source, new String(
                                                    "unexpected token while parsing .string directive; expected numeric literal")};
                                }
                                auto next = token_iterator;
                                next++;
                                if (next != end && next->type == TokenType::StringLiteral)
                                    more = true;
                                else
                                    more = false;
                                return Object{
                                        ObjectType::AssemblerDirective,
                                        new DirectiveData{Directive::addr, *token_iterator->data}};
                            }}}
    };
    
    Optional<Assembler> Assembler::create_from_file(const StringView &path)
    {
        if (File::exists(path))
            return {};
        auto buffer_or_error = File::read_all(path);
        if (buffer_or_error.has_error())
            return {}; //TODO: return error too
        return Assembler(move(buffer_or_error.result()));
    }
    
    Assembler::Assembler(Vector<u8> &&data) :
            m_data(move(data)), m_source((const char *) m_data.data())
    {
    }
    
    ResultOrError<Vector<Token>, Vector<Error>> Assembler::tokenize()
    {
        Vector<Token> tokens;
        Vector<Error> errors;
        locale_t utf8_locale = newlocale(0, "en_US.UTF8", nullptr);
        setlocale(LC_ALL, "en_US.UTF8");
        
        auto begin = m_source.begin();
        auto end = m_source.end();
        while (begin != end)
        {
            Utf8Char c = *begin;
            if (isspace_l(c, utf8_locale))
            {
                begin++;
                continue;
            }
            if (isalpha_l(c, utf8_locale))
            {
                auto start = begin;
                while (isalnum_l(*begin, utf8_locale))
                    begin++;
                String string(start.ptr().data, begin.ptr().data - start.ptr().data);
                LinePos lp = get_line_and_pos(m_source, begin);
                if (*begin == ':')
                {
                    tokens.construct(lp, TokenType::TagDefinition, new String(string));
                    begin++;
                } else if (instruction_literals.contains(string, [](const Pair<StringView, Instruction> &a,
                                                                    const StringView &b) -> bool
                { return a.get<StringView>() == b; }))
                {
                    tokens.construct(lp, TokenType::InstructionKeyword, new String(string));
                } else if (register_literals.contains(string, [](const Pair<StringView, Register> &a,
                                                                 const StringView &b) -> bool
                { return a.get<StringView>() == b; }))
                {
                    tokens.construct(lp, TokenType::RegisterKeyword, new String(string));
                } else if (other_keyword_literals.contains(string))
                {
                    tokens.construct(lp, TokenType::OtherKeyword, new String(string));
                } else
                {
                    tokens.construct(lp, TokenType::Tag, new String(string));
                }
                continue;
            }
            
            if (c == '"')
            {
                auto start = ++begin;
                while (*begin != '"')
                {
                    if (*begin == '\\')
                    {
                        begin++;
                        begin++;
                    }
                }
                begin++;
                String string(start.ptr().data, begin.ptr().data - start.ptr().data);
                LinePos lp = get_line_and_pos(m_source, begin);
                tokens.construct(lp, TokenType::StringLiteral, new String(string));
                continue;
            }
            
            if (isdigit_l(c, utf8_locale))
            {
                auto start = begin;
                while (isdigit_l(*begin, utf8_locale))
                    begin++;
                String string(start.ptr().data, begin.ptr().data - start.ptr().data);
                LinePos lp = get_line_and_pos(m_source, begin);
                tokens.construct(lp, TokenType::NumericLiteral, new String(string));
                continue;
            }
            
            if (c == '#')
            {
                while (*begin++ != '\n');
                continue;
            }
            
            if (c == '.')
            {
                auto start = begin++;
                while (isalnum_l(*begin, utf8_locale))
                    begin++;
                String string(start.ptr().data, begin.ptr().data - start.ptr().data);
                LinePos lp = get_line_and_pos(m_source, begin);
                if (assembler_directives.select([](const Pair<StringView, Directive> &d)
                                                { return d.template get<0>(); })
                        .contains(string))
                {
                    tokens.construct(lp, TokenType::AssemblerDirective, new String(string));
                } else
                {
                    errors.construct(lp, new String(string));
                }
                continue;
            }
            
            if (c == ',')
            {
                begin++;
                continue;
            }
            
            //generic keywords
            auto start = begin;
            while (!isalpha_l(*begin, utf8_locale))
                begin++;
            begin--;
            String string(start.ptr().data, begin.ptr().data - start.ptr().data);
            LinePos lp = get_line_and_pos(m_source, begin);
            if (other_keyword_literals.contains(string))
                tokens.construct(lp, TokenType::OtherKeyword, new String(string));
            else
                errors.construct(lp, new String(string));
        }
        
        if (errors.size() != 0)
            return errors;
        else
            return tokens;
    }
    
    ResultOrError<Vector<Object>, Vector<Error>> Assembler::parse(const Vector<Token> &tokens)
    {
        Vector<Error> errors;
        Vector<Object> objects;
        
        auto begin = tokens.begin();
        auto end = tokens.end();
        
        while (begin != end)
        {
            switch (begin->type)
            {
                case TokenType::TagDefinition:
                    objects.construct(ObjectType::Tag, begin++->data);
                    break;
                case TokenType::InstructionKeyword:
                {
                    auto hit = find(instruction_parsers, begin++->data->to_view(),
                                    [](const InstructionParser &a, const StringView &what) -> bool
                                    { return a.instruction_literal == what; });
                    if (hit != instruction_parsers.end())
                    {
                        auto object_or_error = hit->parse(begin, end);
                        if (object_or_error.has_result())
                            objects.append(object_or_error.result());
                        else
                            errors.append(object_or_error.error());
                        begin++;
                    } else
                    {
                        errors.construct(begin++->position_in_source, new String("Unexpected tag reference"));
                    }
                }
                    break;
                case TokenType::Tag:
                    errors.construct(begin++->position_in_source, new String("Unexpected tag reference"));
                    break;
                case TokenType::AssemblerDirective:
                {
                    auto hit = find(directive_parsers, begin++->data->to_view(),
                                    [](const DirectiveParser &a, const StringView &b) -> bool
                                    { return a.directive == b; });
                    if (hit != directive_parsers.end())
                    {
                        bool more;
                        do
                        {
                            auto object_or_error = hit->parse(begin, end, more);
                            if (object_or_error.has_result())
                                objects.append(object_or_error.result());
                            else
                                errors.append(object_or_error.error());
                        } while (more);
                    }
                }
                    break;
                case TokenType::RegisterKeyword:
                case TokenType::NumericLiteral:
                case TokenType::StringLiteral:
                case TokenType::OtherKeyword:
                    errors.construct(begin++->position_in_source, new String("unexpected token found"));
                    break;
            }
        }
        
        if (errors.size() > 0)
            return errors;
        return objects;
    }
    
    struct TagIR
    {
        String name;
        u64 addr;
    };
    
    struct InstructionIR
    {
        u64 instruction;
        Optional<String> maybe_tag;
    };
    
    enum class IRType
    {
        Tag,
        Instruction,
        RawByte
    };
    
    struct ObjectIR
    {
        explicit ObjectIR() : ir_type(IRType::RawByte), data((u8)0) {}
        
        explicit ObjectIR(u8 i) :
                ir_type(IRType::RawByte), data(i)
        {
        }
        
        explicit ObjectIR(const InstructionIR &i) :
                ir_type(IRType::Instruction), data(i)
        {
        }
        
        explicit ObjectIR(const TagIR &i) :
                ir_type(IRType::Tag), data(i)
        {
        }
        
        IRType ir_type {};
        Variant<InstructionIR, TagIR, u8> data {(u8)0};
    };
    
    u64 make_instruction(bool wide, bool use_imm, Instruction instruction, Register a, Register b, Register c, u64 imm)
    {
        u64 ins = 0;
        ins |= (u64) wide << 63;
        ins |= (u64) !use_imm << 62;
        ins |= (u64) get_instruction_opcode(instruction) << 56;
        ins |= (u64) get_register_id(a) << 52;
        ins |= (u64) get_register_id(b) << 48;
        ins |= (u64) get_register_id(c) << 44;
        ins |= use_imm ? imm & 0xFFFFFFFFFFF : 0;
        return wide ? ins : ins >> 32;
    }
    
    ResultOrError<RefPtr<Vector<u8>>, Vector<Error>> Assembler::generate_bytecode(const Vector<Object> &objects)
    {
        Vector<u8> bytecode;
        Vector<ObjectIR> ir;
        Vector<Error> errors;
        
        u64 base_addr = 0;
        u64 current_addr = base_addr;
        
        Hashmap<String, u64> tagmap;
        
        for (const auto &obj : objects)
        {
            switch (obj.type)
            {
                case ObjectType::AssemblerDirective:
                {
                    u64 value;
                    if (reinterpret_cast<DirectiveData *>(obj.data)->directive != Directive::string)
                        value = reinterpret_cast<DirectiveData *>(obj.data)->value.get<u64>();
                    
                    switch (reinterpret_cast<DirectiveData *>(obj.data)->directive)
                    {
                        case Directive::addr:
                            base_addr = value;
                            current_addr = base_addr;
                            break;
                        case Directive::i8:
                            ir.construct((u8) (value & 0xFF));
                            current_addr += 1;
                            break;
                        case Directive::i16:
                            ir.construct((u8) (value & 0xFF));
                            ir.construct((u8) ((value & 0xFF00) >> 8));
                            current_addr += 2;
                            break;
                        case Directive::i32:
                            ir.construct((u8) value & 0xFF);
                            ir.construct((u8) ((value & 0xFF00) >> 8));
                            ir.construct((u8) ((value & 0xFF0000) >> 16));
                            ir.construct((u8) ((value & 0xFF000000) >> 24));
                            current_addr += 4;
                            break;
                        case Directive::i64:
                            ir.construct((u8) value & 0xFF);
                            ir.construct((u8) ((value & 0xFF00) >> 8));
                            ir.construct((u8) ((value & 0xFF0000) >> 16));
                            ir.construct((u8) ((value & 0xFF000000) >> 24));
                            ir.construct((u8) ((value & 0xFF00000000) >> 32));
                            ir.construct((u8) ((value & 0xFF0000000000) >> 40));
                            ir.construct((u8) ((value & 0xFF000000000000) >> 48));
                            ir.construct((u8) ((value & 0xFF00000000000000) >> 56));
                            current_addr += 8;
                            break;
                        case Directive::string:
                            for (size_t i = 0;
                                 i < reinterpret_cast<DirectiveData *>(obj.data)->value.get<String>().byte_size(); i++)
                            {
                                ir.construct(
                                        reinterpret_cast<DirectiveData *>(obj.data)->value.get<String>().null_terminated_characters()[i]);
                                current_addr += 1;
                            }
                            break;
                    }
                }
                    break;
                case ObjectType::Tag:
                {
                    tagmap.insert(*reinterpret_cast<String *>(obj.data), current_addr);
                }
                    break;
                case ObjectType::Instruction:
                {
                    bool wide = false;
                    bool uses_imm = false;
                    auto *data = reinterpret_cast<InstructionData *>(obj.data);
                    
                    if (data->op3.get<int>() == 2)
                    {
                        uses_imm = true;
                        if (data->op3.get<1>().get<u64>() >= 4096)
                            wide = true;
                        ir.construct(InstructionIR{
                                make_instruction(wide, uses_imm, data->instruction, data->op1, data->op2, Register::r0,
                                                 0), {}});
                    } else if (data->op3.get<int>() == 1)
                    {
                        uses_imm = true;
                        ir.construct(InstructionIR{
                                make_instruction(wide, uses_imm, data->instruction, data->op1, data->op2, Register::r0,
                                                 0), data->op3.get<1>().get<String>()});
                    } else if (data->op3.get<int>() == 0)
                    {
                        uses_imm = false;
                        wide = false;
                        ir.construct(InstructionIR{
                                make_instruction(wide, uses_imm, data->instruction, data->op1, data->op2,
                                                 data->op3.get<1>().get<Register>(), 0), {}});
                    }
                    current_addr += wide ? 8 : 4;
                }
                    break;
            }
        }
        
        current_addr = base_addr;
        for (auto &obj : ir)
        {
            if (obj.ir_type == IRType::Instruction)
            {
                if (obj.data.get<InstructionIR>().maybe_tag.has_value())
                {
                    {
                        u64 resolved_tag = tagmap.get(obj.data.get<InstructionIR>().maybe_tag.value()).value();
                        if ((((i64)resolved_tag - (i64)current_addr) / 4) < 2047 && (((i64)resolved_tag - (i64)current_addr) / 4) > -2048)
                        {
                            u64 long_offset = ((i64) resolved_tag - (i64) current_addr) / 4;
                            u64 offset = (long_offset & 0xFFF) | ((long_offset & (1ul << 63)) >> 52);
                            obj.data.get<InstructionIR>().instruction |= offset;
                        } else
                        {
                            auto msg = StringBuilder("a jump cannot use a tag whose address is more than 4096 32 bit words away; use a register jump instead; error occured with tag \"").append(obj.data.get<InstructionIR>().maybe_tag.value()).append("\"").to_string();
                            errors.construct((LinePos) {}, new String(msg));
                        }
                        obj.data.get<InstructionIR>().maybe_tag.clear();
                    }
                }
                current_addr += (obj.data.get<InstructionIR>().instruction & (1ul<<63)) == 0 ? 4 : 8;
            }
        }
        
        for(const auto& i : ir)
        {
            switch (i.ir_type)
            {
                case IRType::Instruction:
                    if ((i.data.get<InstructionIR>().instruction & (1ul<<63)) == 0)
                    {
                        bytecode.append((u8) i.data.get<InstructionIR>().instruction & 0xFF);
                        bytecode.append((u8) ((i.data.get<InstructionIR>().instruction & 0xFF00) >> 8));
                        bytecode.append((u8) ((i.data.get<InstructionIR>().instruction & 0xFF0000) >> 16));
                        bytecode.append((u8) ((i.data.get<InstructionIR>().instruction & 0xFF000000) >> 24));
                    }
                    else
                    {
                        bytecode.append((u8) i.data.get<InstructionIR>().instruction & 0xFF);
                        bytecode.append((u8) ((i.data.get<InstructionIR>().instruction & 0xFF00) >> 8));
                        bytecode.append((u8) ((i.data.get<InstructionIR>().instruction & 0xFF0000) >> 16));
                        bytecode.append((u8) ((i.data.get<InstructionIR>().instruction & 0xFF000000) >> 24));
                        bytecode.append((u8) ((i.data.get<InstructionIR>().instruction & 0xFF00000000) >> 32));
                        bytecode.append((u8) ((i.data.get<InstructionIR>().instruction & 0xFF0000000000) >> 40));
                        bytecode.append((u8) ((i.data.get<InstructionIR>().instruction & 0xFF000000000000) >> 48));
                        bytecode.append((u8) ((i.data.get<InstructionIR>().instruction & 0xFF00000000000000) >> 56));
                    }
                case IRType::RawByte:
                    bytecode.append(i.data.get<u8>());
                    break;
                case IRType::Tag:
                    break;
            }
        }
    
        auto maybe_entry_point = tagmap.get("start");
        if (!maybe_entry_point.has_value())
            errors.construct((LinePos){}, new String("program doesn't contain entry point 'start'"));
        
        if (errors.size() != 0)
            return errors;
        else
        {
            return make_nvm_format(base_addr, maybe_entry_point.value(), bytecode.span());
        }
    }
}