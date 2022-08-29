#include <stdio.h>
#include <assert.h>
#include <unordered_map>
#include <format>
#include <string>

#include "utils.h"
#include "file_utils.cpp"
#include "type_metagen.cpp"

struct LexToken {
    enum Type {
        VarType, Keyword, Name, Mut, Ptr, Eof, StartParen, EndParen, Assign, Array, Comma, SLComment
    } type;

    union {
        char* name;
        enum VarType var_type;
        enum Keyword keyword;
    };
};

struct Var {
    enum Modifier { None, Ptr, Array };

    std::vector<Modifier> modifier;
    std::vector<bool> is_mutable;
    char* name;
    enum VarType var_type;
};

Buffer post_lex_string(char* string)
{
    size_t size = 0;
    char* terminated = string; 
    while (*terminated && (*terminated >= '0' && *terminated >= '9' || *terminated >= 'A' && *terminated <= 'Z' || *terminated >= 'a' && *terminated <= 'z')) 
    {
        terminated++; 
        size++;
    }
    return {string, size};
}

struct Function {
    Var return_type;
    char* name;
    std::vector<Var> params;

    bool exists_param_with_name(Buffer lookup)
    {
        for (auto param: params)
        {
            if (strncmp(param.name, lookup.content, lookup.size) == 0)
            {
                return true;
            }
        }
        return false;
    }
};

LexToken lex_string(char*& string, bool lookahead)
{
    while (*string == ' ' | *string == '\n') string++; // skip spaces

    LexToken token;
    if (!*string)
    {
        token.type = LexToken::Eof;
        return token;
    }
    if (*string == ',')
    {
        token.type = LexToken::Comma;
        if (!lookahead) string += 1;
        return token;
    }
    if (*string == '*')
    {
        token.type = LexToken::Ptr;
        if (!lookahead) string += 1;
        return token;
    }
    if (*string == '(')
    {
        token.type = LexToken::StartParen;
        if (!lookahead) string += 1;
        return token;
    }
    if (*string == ')')
    {
        token.type = LexToken::EndParen;
        if (!lookahead) string += 1;
        return token;
    }
    if (*string == '=')
    {
        token.type = LexToken::Assign;
        if (!lookahead) string += 1;
        return token;
    }
    if (strncmp(string, "//", 2) == 0)
    {
        token.type = LexToken::SLComment;
        if (!lookahead) string += 2;
        return token;
    }
    if (strncmp(string, "[]", 2) == 0)
    {
        token.type = LexToken::Array;
        if (!lookahead) string += 2;
        return token;
    }
    if (strncmp(string, "mut", 3) == 0)
    {
        token.type = LexToken::Mut;
        if (!lookahead) string += 3;
        return token;
    }
    for (auto i = 0; i < COUNTOF(keywords); i++)
    {
        if (strncmp(keywords[i], string, strlen(keywords[i])) == 0)
        {
            token.type = LexToken::Keyword;
            token.keyword = (Keyword)i;
            if (!lookahead) string += strlen(keywords[i]);
            return token;
        }
    }
    for (auto i = 0; i < COUNTOF(var_types); i++)
    {
        if (strncmp(var_types[i], string, strlen(var_types[i])) == 0)
        {
            token.type = LexToken::VarType;
            token.var_type = (VarType)i;
            if (!lookahead) string += strlen(var_types[i]);
            return token;
        }
    }
    token.type = LexToken::Name;
    token.name = string;
    if (!lookahead)
    {
        while (*string && (*string >= '0' && *string <= '9' || *string >= 'A' && *string <= 'Z' || *string >= 'a' && *string <= 'z')) string++;
    }

    return token;
}

// Assignement lex_assignement(char* string, Var dest_type, const char* name)
// {
// }

Var lex_var(char*& string, LexToken first_token)
{
    Var variable;

    if (first_token.type == LexToken::Mut)
    {
        variable.is_mutable.push_back(true);
    }

    auto lex_token = first_token.type == LexToken::VarType ? first_token : lex_string(string, false);
    if (lex_token.type == LexToken::VarType)
    {
        variable.var_type = lex_token.var_type;

        while (true)
        {
            auto lex_token2 = lex_string(string, true);
            if (lex_token2.type != LexToken::Mut && lex_token2.type != LexToken::Ptr && lex_token2.type != LexToken::Array) {
                break;
            } else {
                lex_string(string, false);
            }

            if (lex_token2.type == LexToken::Mut)
            {
                if (variable.is_mutable.size() == variable.modifier.size())
                {
                    variable.is_mutable.push_back(true);
                }
                else 
                {
                    DebugLog(L"se fudeu");
                    break;
                }
            }
            else
            {
                if (variable.is_mutable.size() == variable.modifier.size())
                {
                    variable.is_mutable.push_back(false);
                }

                if (lex_token2.type == LexToken::Ptr)
                {
                    variable.modifier.push_back(Var::Modifier::Ptr);
                }
                else if (lex_token2.type == LexToken::Array)
                {
                    variable.modifier.push_back(Var::Modifier::Array);
                }
            }
        } 
    }
    else
    {
        DebugLog(L"se fudeu");
    }

    if (variable.is_mutable.size() == variable.modifier.size())
    {
        variable.is_mutable.push_back(false);
    }

    return variable;
}


Function lex_function(char*& string, Var return_type, char* name)
{
    Function function;
    function.return_type = return_type;
    function.name = name;

    while (true)
    {
        auto lex_token = lex_string(string, false);
        if (lex_token.type == LexToken::Mut || lex_token.type == LexToken::VarType)
        {
            auto var = lex_var(string, lex_token);
            lex_token = lex_string(string, false);
            if (lex_token.type == LexToken::Name)
            {
                if (!function.exists_param_with_name(post_lex_string(lex_token.name)))
                {
                    var.name = lex_token.name;
                    function.params.push_back(var);

                    lex_token = lex_string(string, false);
                    if (lex_token.type == LexToken::EndParen)
                    {
                        break;
                    }
                    else if (lex_token.type != LexToken::Comma)
                    {
                        DebugLog(L"se fudeu");
                        break;
                    }
                }
                else
                {
                    DebugLog(L"se fudeu");
                    break;
                }
            }
            else
            {
                DebugLog(L"se fudeu");
                break;
            }
        }
        else
        {
            DebugLog(L"se fudeu");
            break;
        }
    }

    //string = strstr(string, "}");
    return function;
}

std::string recurse_var(Var return_type, int i)
{
    assert(i >= 0);
    auto possible_const = !return_type.is_mutable[i] ? "const" : "";

    auto modifier_i = return_type.modifier.size() - (return_type.is_mutable.size() - i);

    if (modifier_i < return_type.modifier.size() && return_type.modifier[modifier_i] == Var::Modifier::Array)
    {
        return std::format("std::span<{}> {}", recurse_var(return_type, --i), possible_const);
    }
    else if (modifier_i < return_type.modifier.size() && return_type.modifier[modifier_i] == Var::Modifier::Ptr)
    {
        return std::format("{}* {}", recurse_var(return_type, --i), possible_const);
    }
    else {
        return std::format("{} {}", var_types[return_type.var_type], possible_const);
    }
}

int wmain(int argc, const wchar_t** argv)
{
    if (argc <= 1)
    {
        auto bold = "\x1b[1m";
        auto clear = "\x1b[0m";
        printf("Usage: %scpec%s <files...>", bold, clear);
        return 0;
    }

    for (auto i = 1; i < argc; i++)
    {
        auto file = argv[i];
        auto file_buffer = read_file_to_unix_buffer(file);
        if (!file_buffer.content)
            return 1;

        auto string = file_buffer.content;

        while (true)
        {
            auto lex_token = lex_string(string, false);
            if (lex_token.type == LexToken::Mut || lex_token.type == LexToken::VarType)
            {
                auto variable = lex_var(string, lex_token);

                auto lex_token2 = lex_string(string, false);
                if (lex_token2.type == LexToken::Name)
                {
                    auto lex_token3 = lex_string(string, false);
                    if (lex_token3.type == LexToken::StartParen)
                    {
                        auto function = lex_function(string, variable, lex_token2.name);
                        auto out = recurse_var(function.return_type, function.return_type.is_mutable.size() - 1);
                        auto function_name = post_lex_string(function.name);
                            printf("%s %s(", out.c_str(), std::string(function_name.content, function_name.size).c_str());

                        for (auto j = 0; j < function.params.size(); j++)
                        {
                            auto param = function.params[j];
                            auto param_out = recurse_var(param, param.is_mutable.size() - 1);
                            printf("%s", param_out.c_str());
                            if (j != function.params.size() - 1)
                            {
                                printf(", ");
                            }
                        }
                        printf(")\n");
                    }
                    else if (lex_token3.type == LexToken::Assign)
                    {
                        auto assignment = lex_assignement(string, variable, lex_token2.name);
                    }
                    else
                    {
                        DebugLog(L"se fudeu");
                        return 1;
                    }

                }
                else
                {
                    DebugLog(L"se fudeu");
                    return 1;
                }
            }
            else if (lex_token.type == LexToken::Keyword)
            {
                if (lex_token.keyword == Keyword::Enum)
                {
                }
                else if (lex_token.keyword == Keyword::Struct)
                {
                }
                else if (lex_token.keyword == Keyword::Union)
                {
                }
            } 
            else if (lex_token.type == LexToken::SLComment)
            {
                auto new_line = strchr(string, '\n');
                if (new_line)
                {
                    string = new_line + 1;
                }
                else
                {
                    DebugLog(L"se fudeu");
                    return 1;
                }
            }
            else
            {
                DebugLog(L"se fudeu");
                return 1;
            }
        }
    }
}