#include <stdio.h>
#include <assert.h>
#include <unordered_map>
#include <format>
#include <string>
#include <stdlib.h>

#include "utils.h"
#include "file_utils.cpp"
#include "type_metagen.cpp"

struct LexToken {
    enum Type {
        VarType, Keyword, Name, Number, Mut, Let, Ptr, Eof, StartParen, EndParen, StartRect, EndRect, StartCurly, EndCurly, Assign, Comma, SLComment, LessThen, BiggerThen, Plus, Minus
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

Buffer buffer_string_ptr(char* string)
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
    if (*string == '+')
    {
        token.type = LexToken::Plus;
        if (!lookahead) string += 1;
        return token;
    }
    if (*string == '-')
    {
        token.type = LexToken::Minus;
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
    if (*string == '[')
    {
        token.type = LexToken::StartRect;
        if (!lookahead) string += 1;
        return token;
    }
    if (*string == ']')
    {
        token.type = LexToken::EndRect;
        if (!lookahead) string += 1;
        return token;
    }
    if (*string == '{')
    {
        token.type = LexToken::StartCurly;
        if (!lookahead) string += 1;
        return token;
    }
    if (*string == '}')
    {
        token.type = LexToken::EndCurly;
        if (!lookahead) string += 1;
        return token;
    }
    if (*string == '=')
    {
        token.type = LexToken::Assign;
        if (!lookahead) string += 1;
        return token;
    }
    if (*string == '<')
    {
        token.type = LexToken::LessThen;
        if (!lookahead) string += 1;
        return token;
    }
    if (*string == '>')
    {
        token.type = LexToken::BiggerThen;
        if (!lookahead) string += 1;
        return token;
    }
    auto candidate = string;
    while (*candidate >= '0' && *candidate <= '9' || *candidate == '.') candidate++;
    if (!(*candidate >= 'A' && *candidate <= 'Z' || *candidate >= 'a' && *candidate <= 'z'))
    {
        token.type = LexToken::Number;
        token.name = string;
        if (!lookahead) string = candidate;
        return token;
    }

    if (strncmp(string, "//", 2) == 0)
    {
        token.type = LexToken::SLComment;
        if (!lookahead) string += 2;
        return token;
    }
    if (strncmp(string, "mut", 3) == 0)
    {
        token.type = LexToken::Mut;
        if (!lookahead) string += 3;
        return token;
    }
    if (strncmp(string, "let", 3) == 0)
    {
        token.type = LexToken::Let;
        if (!lookahead) string += 3;
        return token;
    }
    for (auto i = 0; i < COUNTOF(keywords); i++)
    {
        if (keywords[i] && strncmp(keywords[i], string, strlen(keywords[i])) == 0)
        {
            token.type = LexToken::Keyword;
            token.keyword = (Keyword)i;
            if (!lookahead) string += strlen(keywords[i]);
            return token;
        }
    }
    for (auto i = 0; i < COUNTOF(var_types); i++)
    {
        if (var_types[i] && strncmp(var_types[i], string, strlen(var_types[i])) == 0)
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

struct VarDecl {
    Var lhs;
    Buffer rhs;
};

VarDecl lex_vardecl_while(char*& string, Var dest_type)
{
    VarDecl var_decl = {.lhs = dest_type};
    while (*string && *string == ' ') string++;
    var_decl.rhs.content = string;

    var_decl.rhs.size = 0;
    while (*string && *string != '\n' && *string != ')' && *string != '{') 
    {
        var_decl.rhs.size++;
        string++;
    }

    return var_decl;
}

VarDecl lex_vardecl(char*& string, Var dest_type)
{
    VarDecl var_decl = {.lhs = dest_type};
    while (*string && *string == ' ') string++;
    var_decl.rhs.content = string;

    auto size = 0;
    while (*string && *string != '\n') 
    {
        size++;
        string++;
    }
    var_decl.rhs.size = size;

    return var_decl;
}

Var lex_var(char*& string, LexToken first_token)
{
    Var variable = {.var_type = VarType::Any};
    LexToken lex_token2;
    auto first_time = true;
    while (true)
    {
        if (first_time)
        {
            lex_token2 = first_token;
        }
        else
        {
            lex_token2 = lex_string(string, true);
            if (lex_token2.type != LexToken::VarType && lex_token2.type != LexToken::Mut && lex_token2.type != LexToken::Ptr && lex_token2.type != LexToken::StartRect && lex_token2.type != LexToken::Let)
            {
                break;
            }
            else
            {
                lex_string(string, false);
            }
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
            else if (lex_token2.type == LexToken::StartRect)
            {
                variable.modifier.push_back(Var::Modifier::Array);
            }
            else if (lex_token2.type == LexToken::VarType)
            {
                variable.var_type = lex_token2.var_type;
            }
            else if (lex_token2.type == LexToken::Let)
            {
                if (!first_time)
                {
                    DebugLog(L"se fudeu");
                    break;
                }
            }
        }

        first_time = false;
    } 

    auto starting_arrays = 0;
    for (auto modifier: variable.modifier) 
    {
        if (modifier == Var::Array)
            starting_arrays++;
    }

    auto closing_arrays = 0;
    while (true)
    {
        if (lex_token2.type == LexToken::EndRect)
        {
            lex_token2 = lex_string(string, false);
            closing_arrays++;
        }
        else
        {
            if (closing_arrays != starting_arrays)
            {
                DebugLog(L"se fudeu");
            }
            break;
        }
        lex_token2 = lex_string(string, true);
    }

    return variable;
}

bool possibly_var(LexToken::Type type)
{
    return type == LexToken::Mut || type == LexToken::VarType || type == LexToken::StartRect || type == LexToken::Let;
}

struct Capsule {
    Buffer pre, post;
    struct Expr* inside;
};

struct Expr {
    enum {
        VarInit, Capsules, Leaf
    } type;
    std::vector<Capsule> capsules;
    struct {
        struct Var dest_var;
        struct Expr* rhs;
    };
    Buffer leaf;
};

// void lex_while_inner(char*& string, struct While& while_statement, bool outer_most)
// {
//     Capsule capsule = {};

//     char* pre_paren = string;

//     while (true)
//     {
//         auto lex_token = lex_string(string, false);
//         if (lex_token.type == LexToken::StartParen)
//         {
//             capsule.start.content = pre_paren;
//             capsule.start.size = string - pre_paren - 1;

//             lex_while_inner(string, while_statement, false);

//             lex_token = lex_string(string, false);
//             if (lex_token.type == LexToken::EndParen)
//             {
//                 capsule.end.content = string;
//                 while (true)
//                 {
//                     lex_token = lex_string(string, false);
//                     if (outer_most && lex_token.type == LexToken::StartCurly || lex_token.type == LexToken::EndParen)
//                     {
//                         capsule.end.size = string - capsule.end.content - 1;
//                         break;
//                     }
//                 }
//                 while_statement.capsules.push_back(capsule);
//                 return;
//             }
//             else
//             {
//                 DebugLog(L"se fudeu");
//                 return;
//             }
//         }
//         else if (possibly_var(lex_token.type))
//         {
//             auto var = lex_var(string, lex_token);
//             lex_token = lex_string(string, false);
//             if (lex_token.type == LexToken::Name)
//             {
//                 var.name = lex_token.name;
//                 lex_token = lex_string(string, false);
//                 if (lex_token.type == LexToken::Assign)
//                 {
//                     auto assignment = lex_vardecl_while(string, var);
//                     while_statement.var_decl = assignment;
//                     return;
//                 }
//                 else
//                 {
//                     DebugLog(L"se fudeu");
//                     return;
//                 }
//             }
//         }
//         else if (lex_token.type == LexToken::EndParen)
//         {
//             if (!outer_most) return;
//             DebugLog(L"se fudeu");
//             return;
//         }
//     }
// }

Expr* lex_expr(char*& string, bool outer_most)
{
    Expr* expr = new Expr;

    char* pre_paren = string;

    while (true)
    {
        auto is_newline = !*string || *string == '\n';
        auto lex_token = lex_string(string, false);
        auto is_eof = lex_token.type == LexToken::Eof;
        if (lex_token.type == LexToken::StartParen)
        {
            expr->type = Expr::Capsules;
            Capsule capsule = {};
            if (outer_most)
            {
                capsule.pre.content = pre_paren;
                capsule.pre.size = string - pre_paren - 1;
            }

            capsule.inside = lex_expr(string, false);

            char* pre_end_paren = string;

            while (true)
            {
                is_newline = *string == '\n';
                lex_token = lex_string(string, false);
                is_eof = lex_token.type == LexToken::Eof;
                if (lex_token.type == LexToken::StartParen)
                {
                    capsule.post.content = pre_end_paren;
                    capsule.post.size = string - pre_end_paren - 1;
                    expr->capsules.push_back(capsule);
                    capsule = {};
                    capsule.inside = lex_expr(string, false);
                    pre_end_paren = string;
                }
                if (outer_most && (is_newline || is_eof) || !outer_most && lex_token.type == LexToken::EndParen)
                {
                    capsule.post.content = pre_end_paren;
                    capsule.post.size = string - pre_end_paren - 1 + (is_eof ? 1: 0);
                    expr->capsules.push_back(capsule);
                    return expr;
                }
            }
        }
        else if (possibly_var(lex_token.type))
        {
            auto variable = lex_var(string, lex_token);

            lex_token = lex_string(string, false);
            if (lex_token.type == LexToken::Name)
            {
                variable.name = lex_token.name;
                lex_token = lex_string(string, false);
                if (lex_token.type == LexToken::Assign)
                {
                    expr->type = Expr::VarInit;
                    expr->dest_var = variable;
                    expr->rhs = lex_expr(string, false);
                    break;
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
        else if (outer_most && (is_newline || is_eof) || !outer_most && lex_token.type == LexToken::EndParen)
        {
            expr->type = Expr::Leaf;
            expr->leaf.content = pre_paren;
            expr->leaf.size = string - pre_paren - 1;
            break;
        }
    }

    return expr;
}

bool is_newline_next(char* string)
{
    auto is_newline_next = false;
    while (*string)
    {
        if (*string == '\n')
        {
            is_newline_next = true;
            break;
        }
    }
    return is_newline_next;
}

std::string recurse_var(Var return_type, int i)
{
    assert(i < return_type.is_mutable.size());
    auto possible_const = !return_type.is_mutable[i] ? " const" : "";

    if (return_type.var_type == VarType::Any)
    {
        return std::format("auto{}", possible_const);
    }

    if (i < return_type.modifier.size() && return_type.modifier[i] == Var::Modifier::Array)
    {
        return std::format("std::span<{}>{}", recurse_var(return_type, ++i), possible_const);
    }
    else if (i < return_type.modifier.size() && return_type.modifier[i] == Var::Modifier::Ptr)
    {
        return std::format("{}*{}", recurse_var(return_type, ++i), possible_const);
    }
    else {
        return std::format("{}{}", var_types[return_type.var_type], possible_const);
    }
}


std::string recurse_expr(Expr expr, int i)
{
    if (expr.type == Expr::Capsules)
    {
        std::string out;
        for (auto j = 0; j < expr.capsules.size(); j++)
        {
            auto start = expr.capsules[j].pre;
            auto end = expr.capsules[j].post;
            if (start.content)
                out += std::string(start.content, start.size).c_str();
            out += std::format("({}){}", recurse_expr(*expr.capsules[j].inside, 0).c_str(), std::string(end.content, end.size).c_str());
        }
        return out;
    }
    else if (expr.type == Expr::Leaf)
    {
        return std::string(expr.leaf.content, expr.leaf.size);
    }
    auto function_name = buffer_string_ptr(expr.dest_var.name);
    auto expr_out = recurse_expr(*expr.rhs, 0);
    return std::format("{} {} = {}", recurse_var(expr.dest_var, 0).c_str(), std::string(function_name.content, function_name.size).c_str(), expr_out.c_str());
}

Function lex_function(char*& string, Var return_type, char* name)
{
    Function function;
    function.return_type = return_type;
    function.name = name;

    while (true)
    {
        auto lex_token = lex_string(string, false);
        if (possibly_var(lex_token.type))
        {
            auto var = lex_var(string, lex_token);
            lex_token = lex_string(string, false);
            if (lex_token.type == LexToken::Name)
            {
                if (!function.exists_param_with_name(buffer_string_ptr(lex_token.name)))
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
            if (lex_token.type == LexToken::EndParen)
            {
                break;
            }
            else
            {
                DebugLog(L"se fudeu");
                break;
            }
        }
    }

    auto lex_token = lex_string(string, false);
    if (lex_token.type == LexToken::StartCurly)
    {
        while (true)
        {
            lex_token = lex_string(string, false);
            if (possibly_var(lex_token.type))
            {
                auto variable = lex_var(string, lex_token);

                lex_token = lex_string(string, false);
                if (lex_token.type == LexToken::Name)
                {
                    variable.name = lex_token.name;
                    if (lex_token.type == LexToken::Assign || is_newline_next(string))
                    {
                        auto assignment = lex_vardecl(string, variable);
                    }
                }
            }
            else if (lex_token.type == LexToken::Keyword)
            {
                if (lex_token.keyword == Keyword::While)
                {
                    // auto while_statement = lex_while(string);
                    // auto out = recurse_while(while_statement, 0);
                    // printf("while (%s) {}\n", out.c_str());
                }
            }
            else if (lex_token.type == LexToken::EndCurly)
            {
                break;
            }
        }
    }
    else
    {
        DebugLog(L"se fudeu");
    }

    return function;
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
            if (possibly_var(lex_token.type))
            {
                auto variable = lex_var(string, lex_token);

                auto lex_token2 = lex_string(string, false);
                if (lex_token2.type == LexToken::Name)
                {
                    auto lex_token3 = lex_string(string, false);
                    if (lex_token3.type == LexToken::StartParen)
                    {
                        auto function = lex_function(string, variable, lex_token2.name);
                        auto out = recurse_var(function.return_type, 0);
                        auto function_name = buffer_string_ptr(function.name);
                        printf("%s %s(", out.c_str(), std::string(function_name.content, function_name.size).c_str());

                        for (auto j = 0; j < function.params.size(); j++)
                        {
                            auto param = function.params[j];
                            auto param_out = recurse_var(param, 0);
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
                        variable.name = lex_token2.name;
                        auto out = recurse_var(variable, 0);

                        auto expr = lex_expr(string, true);
                        auto expr_out = recurse_expr(*expr, 0);

                        auto var_name = buffer_string_ptr(variable.name);
                        printf("%s %s = %s", out.c_str(), std::string(var_name.content, var_name.size).c_str(), expr_out.c_str());
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