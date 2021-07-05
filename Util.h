#pragma once
#include <Types.h>
#include <StringView.h>

namespace nvm
{
    struct LinePos
    {
        size_t line;
        size_t pos;
    };
    
    inline LinePos get_line_and_pos(const StringView& text, const StringViewBidIt& what)
    {
        size_t lines {0};
        size_t codepoints_this_line {0};
        auto begin = text.begin();
        while (begin != what)
        {
            if (*begin++ == '\n')
            {
                lines++;
                codepoints_this_line = 0;
            }
            codepoints_this_line++;
        }
        
        return {lines, codepoints_this_line};
    }
}