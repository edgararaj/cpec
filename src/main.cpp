#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#include <unordered_map>
#include <format>
#include <string>
#include <span>

#include "utils.h"
#include "file_utils.cpp"
#include "type_metagen.cpp"

struct LexToken {
    enum Type {
        VarType, Keyword, Name, Number, Mut, Let, Multiply, Eof, StartParen, EndParen, StartRect, EndRect, StartCurly, EndCurly, Assign, Comma, SLComment, LessThen, BiggerThen, Plus, Minus
    } type;

    union {
        char* name;
        enum VarType var_type;
        enum Keyword keyword;
    };
    int string_size;
};

struct LexBuffer {
    Buffer buffer;
    char* string;
    const wchar_t* file_path;
    int line_num;
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

LexToken lex_string(LexBuffer& lex_buffer, bool lookahead)
{
    while (true)
    {
        if (*lex_buffer.string == ' ' | *lex_buffer.string == '\n')
        {
            if (*lex_buffer.string == '\n')
            {
                lex_buffer.line_num++;
            }
            lex_buffer.string++;
        } 
        else
        {
            break;
        }
    }

    LexToken token;
    token.string_size = 0;

    if (!*lex_buffer.string)
    {
        token.type = LexToken::Eof;
        return token;
    }
    if (*lex_buffer.string == ',')
    {
        token.type = LexToken::Comma;
        if (!lookahead) lex_buffer.string += 1;
        token.string_size = 1;
        return token;
    }
    if (*lex_buffer.string == '+')
    {
        token.type = LexToken::Plus;
        if (!lookahead) lex_buffer.string += 1;
        token.string_size = 1;
        return token;
    }
    if (*lex_buffer.string == '-')
    {
        token.type = LexToken::Minus;
        if (!lookahead) lex_buffer.string += 1;
        token.string_size = 1;
        return token;
    }
    if (*lex_buffer.string == '*')
    {
        token.type = LexToken::Multiply;
        if (!lookahead) lex_buffer.string += 1;
        token.string_size = 1;
        return token;
    }
    if (*lex_buffer.string == '(')
    {
        token.type = LexToken::StartParen;
        if (!lookahead) lex_buffer.string += 1;
        token.string_size = 1;
        return token;
    }
    if (*lex_buffer.string == ')')
    {
        token.type = LexToken::EndParen;
        if (!lookahead) lex_buffer.string += 1;
        token.string_size = 1;
        return token;
    }
    if (*lex_buffer.string == '[')
    {
        token.type = LexToken::StartRect;
        if (!lookahead) lex_buffer.string += 1;
        token.string_size = 1;
        return token;
    }
    if (*lex_buffer.string == ']')
    {
        token.type = LexToken::EndRect;
        if (!lookahead) lex_buffer.string += 1;
        token.string_size = 1;
        return token;
    }
    if (*lex_buffer.string == '{')
    {
        token.type = LexToken::StartCurly;
        if (!lookahead) lex_buffer.string += 1;
        token.string_size = 1;
        return token;
    }
    if (*lex_buffer.string == '}')
    {
        token.type = LexToken::EndCurly;
        if (!lookahead) lex_buffer.string += 1;
        token.string_size = 1;
        return token;
    }
    if (*lex_buffer.string == '=')
    {
        token.type = LexToken::Assign;
        if (!lookahead) lex_buffer.string += 1;
        token.string_size = 1;
        return token;
    }
    if (*lex_buffer.string == '<')
    {
        token.type = LexToken::LessThen;
        if (!lookahead) lex_buffer.string += 1;
        token.string_size = 1;
        return token;
    }
    if (*lex_buffer.string == '>')
    {
        token.type = LexToken::BiggerThen;
        if (!lookahead) lex_buffer.string += 1;
        token.string_size = 1;
        return token;
    }
    if (strncmp(lex_buffer.string, "//", 2) == 0)
    {
        token.type = LexToken::SLComment;
        if (!lookahead) lex_buffer.string += 2;
        token.string_size = 2;
        return token;
    }
    if (strncmp(lex_buffer.string, "mut", 3) == 0)
    {
        token.type = LexToken::Mut;
        if (!lookahead) lex_buffer.string += 3;
        token.string_size = 3;
        return token;
    }
    if (strncmp(lex_buffer.string, "let", 3) == 0)
    {
        token.type = LexToken::Let;
        if (!lookahead) lex_buffer.string += 3;
        token.string_size = 3;
        return token;
    }

    auto candidate = lex_buffer.string;
    while (*candidate >= '0' && *candidate <= '9' || *candidate == '.') candidate++;
    if (!(*candidate >= 'A' && *candidate <= 'Z' || *candidate >= 'a' && *candidate <= 'z'))
    {
        token.type = LexToken::Number;
        token.name = lex_buffer.string;
        token.string_size = candidate - lex_buffer.string;
        if (!lookahead) lex_buffer.string = candidate;
        return token;
    }

    for (auto i = 0; i < COUNTOF(keywords); i++)
    {
        if (keywords[i] && strncmp(keywords[i], lex_buffer.string, strlen(keywords[i])) == 0)
        {
            token.type = LexToken::Keyword;
            token.keyword = (Keyword)i;
            token.string_size = strlen(keywords[i]);
            if (!lookahead) lex_buffer.string += strlen(keywords[i]);
            return token;
        }
    }
    for (auto i = 0; i < COUNTOF(var_types); i++)
    {
        if (var_types[i] && strncmp(var_types[i], lex_buffer.string, strlen(var_types[i])) == 0)
        {
            token.type = LexToken::VarType;
            token.var_type = (VarType)i;
            token.string_size = strlen(var_types[i]);
            if (!lookahead) lex_buffer.string += strlen(var_types[i]);
            return token;
        }
    }
    token.type = LexToken::Name;
    token.name = lex_buffer.string;
    while (*lex_buffer.string && (*lex_buffer.string >= '0' && *lex_buffer.string <= '9' || *lex_buffer.string >= 'A' && *lex_buffer.string <= 'Z' || *lex_buffer.string >= 'a' && *lex_buffer.string <= 'z')) {
        token.string_size++;
        lex_buffer.string++;
    }

    if (lookahead)
    {
        lex_buffer.string = token.name;
    }

    return token;
}

struct VarDecl {
    Var lhs;
    Buffer rhs;
};

char* next_line(char* string)
{
    char* result;
    auto new_line = strchr(string, '\n');
    if (new_line)
    {
        result = new_line + 1;
    }
    else
    {
        result = string + strlen(string);
    }
    return result;
}


void print_error(LexBuffer lex_buffer, LexToken lex_token, const char* msg, const char* line)
{
    auto new_line = lex_buffer.string;
    while (new_line --> lex_buffer.buffer.content)
    {
        if (*new_line == '\n' || new_line == lex_buffer.buffer.content)
        {
            auto gray_fg = "\x1b[90m";
            auto red_fg = "\x1b[31m";
            auto clear_fg = "\x1b[39m";
            auto token_col = lex_buffer.string - new_line - lex_token.string_size + 1;
            printf("%s%ls:%d:%d: %serror: ", gray_fg, lex_buffer.file_path, lex_buffer.line_num, (int)(token_col), red_fg);
            printf("%s%s%s\n", gray_fg, msg, clear_fg);
            printf(" %d | ", lex_buffer.line_num);
            if (!line)
            {
                auto red_underline = "\x1b[4m\x1b[31m";
                auto clear_red_underline = "\x1b[39m\x1b[24m";
                printf("%s%s%s%s%s\n", std::string(new_line + (*new_line == '\n'), token_col - 1).c_str(), red_underline, std::string(lex_buffer.string - lex_token.string_size, lex_token.string_size).c_str(), clear_red_underline, std::string(lex_buffer.string, next_line(lex_buffer.string) - lex_buffer.string).c_str());
            }
            else
            {
                printf("%s\n", line);
            }
        }
    }
}

void print_expectation_error(LexBuffer lex_buffer, LexToken lex_token, std::vector<const char*> expectations)
{
    auto new_line = lex_buffer.string;
    while (new_line --> lex_buffer.buffer.content)
    {
        if (*new_line == '\n' || new_line == lex_buffer.buffer.content)
        {
            std::string line;
            auto msg = std::format("expected \"{}\"", expectations[0]);
            for (auto i = 1; i < expectations.size(); i++)
            {
                msg += std::format(" or \"{}\"", expectations[i]);
            }

            if (lex_token.string_size == 0)
            {
                auto red_fg = "\x1b[31m";
                auto clear_fg = "\x1b[39m";
                auto gray_fg = "\x1b[90m";

                assert(expectations.size() > 0);
                std::string expectation_str = std::format("{}{}", red_fg, expectations[0]);
                for (auto i = 1; i < expectations.size(); i++)
                {
                    expectation_str += std::format("{}/{}{}", gray_fg, red_fg, expectations[i]);
                }

                line = std::format("{} {}{}", std::string(new_line + (*new_line == '\n'), lex_buffer.string - new_line).c_str(), expectation_str.c_str(), clear_fg);
            }

            print_error(lex_buffer, lex_token, msg.c_str(), line.size() > 0 ? line.c_str() : 0);
        }
    }
}




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

template <typename T>
struct ErrorOr
{
    ErrorOr()
    {
        error = true;
    }

    ErrorOr(T ok)
    {
        error = false;
        content = ok;
    }

    T content;
    bool error;
};

ErrorOr<Var> lex_var(LexBuffer& lex_buffer, LexToken first_token)
{
    Var variable = {.var_type = VarType::Any};
    LexToken lex_token;
    auto first_time = true;
    while (true)
    {
        if (first_time)
        {
            lex_token = first_token;
        }
        else
        {
            lex_token = lex_string(lex_buffer, true);
            if (lex_token.type != LexToken::VarType && lex_token.type != LexToken::Mut && lex_token.type != LexToken::LessThen && lex_token.type != LexToken::StartRect && lex_token.type != LexToken::Let)
            {
                break;
            }
            else
            {
                lex_string(lex_buffer, false);
            }
        }

        if (lex_token.type == LexToken::Mut)
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

            if (lex_token.type == LexToken::LessThen)
            {
                variable.modifier.push_back(Var::Modifier::Ptr);
            }
            else if (lex_token.type == LexToken::StartRect)
            {
                variable.modifier.push_back(Var::Modifier::Array);
            }
            else if (lex_token.type == LexToken::VarType)
            {
                variable.var_type = lex_token.var_type;
            }
            else if (lex_token.type == LexToken::Let)
            {
                if (!first_time)
                {
                    print_error(lex_buffer, lex_token, "unexpected \"let\"", 0);
                    break;
                }
            }
        }

        first_time = false;
    } 

    auto starting_arrays = 0;
    auto starting_ptrs = 0;
    for (auto modifier: variable.modifier) 
    {
        if (modifier == Var::Ptr)
            starting_ptrs++;
        else if (modifier == Var::Array)
            starting_arrays++;
    }

    auto lex_token2 = lex_token;

    auto closing_arrays = 0;
    auto closing_ptrs = 0;
    while (true)
    {
        if (lex_token2.type == LexToken::EndRect)
        {
            lex_token2 = lex_string(lex_buffer, false);
            closing_arrays++;
        }
        else if (lex_token2.type == LexToken::BiggerThen)
        {
            lex_token2 = lex_string(lex_buffer, false);
            closing_ptrs++;
        }
        else
        {
            if (closing_arrays != starting_arrays || closing_ptrs != starting_ptrs)
            {
                if (lex_token.type == LexToken::Name)
                {
                    lex_string(lex_buffer, false);
                    print_error(lex_buffer, lex_token, "unknown token", 0);
                }
                else
                {
                    if (closing_arrays != starting_arrays)
                        print_error(lex_buffer, lex_token2, "wrong number of ending ]", 0);
                    else
                        print_error(lex_buffer, lex_token2, "wrong number of ending >", 0);
                }
                return {};
            }
            break;
        }
        lex_token2 = lex_string(lex_buffer, true);
    }

    return variable;
}

bool possibly_var(LexToken::Type type)
{
    return type == LexToken::Mut || type == LexToken::VarType || type == LexToken::StartRect || type == LexToken::Let || type == LexToken::LessThen;
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
//         auto lex_token = lex_string(lex_buffer, false);
//         if (lex_token.type == LexToken::StartParen)
//         {
//             capsule.start.content = pre_paren;
//             capsule.start.size = string - pre_paren - 1;

//             lex_while_inner(string, while_statement, false);

//             lex_token = lex_string(lex_buffer, false);
//             if (lex_token.type == LexToken::EndParen)
//             {
//                 capsule.end.content = string;
//                 while (true)
//                 {
//                     lex_token = lex_string(lex_buffer, false);
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
//             lex_token = lex_string(lex_buffer, false);
//             if (lex_token.type == LexToken::Name)
//             {
//                 var.name = lex_token.name;
//                 lex_token = lex_string(lex_buffer, false);
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
ErrorOr<Expr*> lex_expr(LexBuffer& lex_buffer, bool outer_most)
{
    Expr* expr = new Expr;

    while (*lex_buffer.string == ' ') lex_buffer.string++; // skip spaces
    char* pre_paren = lex_buffer.string;

    while (true)
    {
        auto is_newline = !*lex_buffer.string || *lex_buffer.string == '\n';
        auto lex_token = lex_string(lex_buffer, true);
        is_newline = is_newline || lex_token.type == LexToken::StartCurly;
        auto is_eof = lex_token.type == LexToken::Eof;
        if (!is_newline && !is_eof)
        {
            lex_string(lex_buffer, false);
            if (lex_token.type == LexToken::StartParen)
            {
                expr->type = Expr::Capsules;
                Capsule capsule = {};
                if (outer_most)
                {
                    capsule.pre.content = pre_paren;
                    capsule.pre.size = lex_buffer.string - pre_paren - 1;
                }

                auto error = lex_expr(lex_buffer, false);
                if (error.error)
                    return {};
                capsule.inside = error.content;

                char *pre_end_paren = lex_buffer.string;

                while (true)
                {
                    is_newline = !*lex_buffer.string || *lex_buffer.string == '\n';
                    lex_token = lex_string(lex_buffer, true);
                    is_newline = is_newline || lex_token.type == LexToken::StartCurly;
                    is_eof = lex_token.type == LexToken::Eof;
                    if (!is_newline && !is_eof)
                    {
                        lex_string(lex_buffer, false);
                        if (lex_token.type == LexToken::StartParen)
                        {
                            capsule.post.content = pre_end_paren;
                            capsule.post.size = lex_buffer.string - pre_end_paren - 1;
                            expr->capsules.push_back(capsule);
                            capsule = {};
                            auto error = lex_expr(lex_buffer, false);
                            if (error.error)
                                return {};
                            capsule.inside = error.content;
                            pre_end_paren = lex_buffer.string;
                        }
                        else if (!outer_most && lex_token.type == LexToken::EndParen)
                        {
                            capsule.post.content = pre_end_paren;
                            capsule.post.size = lex_buffer.string - pre_end_paren + (is_eof ? 1 : 0);
                            expr->capsules.push_back(capsule);
                            return expr;
                        }

                    }
                    else if (outer_most)
                    {
                        capsule.post.content = pre_end_paren;
                        capsule.post.size = lex_buffer.string - pre_end_paren - 1 + (is_eof ? 1 : 0);
                        expr->capsules.push_back(capsule);
                        return expr;
                    }
                }
            }
            else if (lex_token.type == LexToken::Mut || lex_token.type == LexToken::Let)
            {
                Var variable = {.var_type = VarType::Any};
                variable.is_mutable.push_back(lex_token.type == LexToken::Mut);

                lex_token = lex_string(lex_buffer, false);
                if (lex_token.type == LexToken::Name)
                {
                    variable.name = lex_token.name;
                    lex_token = lex_string(lex_buffer, false);
                    if (lex_token.type == LexToken::Assign)
                    {
                        expr->type = Expr::VarInit;
                        expr->dest_var = variable;
                        auto error = lex_expr(lex_buffer, false);
                        if (error.error)
                            return {};
                        expr->rhs = error.content;
                        break;
                    }
                    else
                    {
                        print_expectation_error(lex_buffer, lex_token, {"="});
                        break;
                    }
                }
                else
                {
                    print_expectation_error(lex_buffer, lex_token, {"name"});
                    break;
                }
            }
            else if (!outer_most && lex_token.type == LexToken::EndParen)
            {
                expr->type = Expr::Leaf;
                expr->leaf.content = pre_paren;
                expr->leaf.size = lex_buffer.string - pre_paren - 1;
                break;
            }
        }
        else if (outer_most)
        {
            expr->type = Expr::Leaf;
            expr->leaf.content = pre_paren;
            expr->leaf.size = lex_buffer.string - pre_paren;
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
    printf("%s %s;\n", recurse_var(expr.dest_var, 0).c_str(), std::string(function_name.content, function_name.size).c_str());
    return std::format("{} = {}", std::string(function_name.content, function_name.size).c_str(), expr_out.c_str());
}

Function lex_function(LexBuffer& lex_buffer, Var return_type)
{
    Function function;
    function.return_type = return_type;
    function.name = return_type.name;

    while (true)
    {
        auto lex_token = lex_string(lex_buffer, false);
        if (possibly_var(lex_token.type))
        {
            auto error = lex_var(lex_buffer, lex_token);
            if (error.error) break;
            auto var = error.content;
            lex_token = lex_string(lex_buffer, false);
            if (lex_token.type == LexToken::Name)
            {
                if (!function.exists_param_with_name(buffer_string_ptr(lex_token.name)))
                {
                    var.name = lex_token.name;
                    function.params.push_back(var);

                    lex_token = lex_string(lex_buffer, false);
                    if (lex_token.type == LexToken::EndParen)
                    {
                        break;
                    }
                    else if (lex_token.type != LexToken::Comma)
                    {
                        print_expectation_error(lex_buffer, lex_token, {")", ","});
                        break;
                    }
                }
                else
                {
                    print_error(lex_buffer, lex_token, "function parameter is repeated", 0);
                    return function;
                }
            }
            else
            {
                print_expectation_error(lex_buffer, lex_token, {"name"});
                return function;
            }
        }
        else
        {
            if (function.params.size() == 0 && lex_token.type == LexToken::EndParen)
            {
                break;
            }
            else
            {
                print_expectation_error(lex_buffer, lex_token, {"variable type"});
                return function;
            }
        }
    }

    auto lex_token = lex_string(lex_buffer, false);
    if (lex_token.type == LexToken::StartCurly)
    {
        while (true)
        {
            lex_token = lex_string(lex_buffer, false);
            if (possibly_var(lex_token.type))
            {
                auto error = lex_var(lex_buffer, lex_token);
                if (error.error) break;
                auto variable = error.content;

                lex_token = lex_string(lex_buffer, false);
                if (lex_token.type == LexToken::Name)
                {
                    variable.name = lex_token.name;
                    if (lex_token.type == LexToken::Assign || is_newline_next(lex_buffer.string))
                    {
                        //auto assignment = lex_vardecl(string, variable);
                    }
                }
            }
            else if (lex_token.type == LexToken::Keyword)
            {
                if (lex_token.keyword == Keyword::While)
                {
                    auto error = lex_expr(lex_buffer, true);
                    if (error.error) break;
                    auto expr = error.content;
                    auto out = recurse_expr(*expr, 0);
                    printf("while (%s) {}\n", out.c_str());
                }
            }
            else if (lex_token.type == LexToken::EndCurly)
            {
                break;
            }
            else if (lex_token.type == LexToken::Eof)
            {
                print_expectation_error(lex_buffer, lex_token, {"}"});
                break;
            }
        }
    }
    else
    {
        print_expectation_error(lex_buffer, lex_token, {"{"});
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

        LexBuffer lex_buffer;
        lex_buffer.buffer = file_buffer;
        lex_buffer.file_path = file;
        lex_buffer.line_num = 1;
        lex_buffer.string = lex_buffer.buffer.content;

        while (true)
        {
            auto lex_token = lex_string(lex_buffer, false);
            if (possibly_var(lex_token.type))
            {
                auto error = lex_var(lex_buffer, lex_token);
                if (error.error) break;
                auto variable = error.content;

                lex_token = lex_string(lex_buffer, false);
                if (lex_token.type == LexToken::Name)
                {
                    variable.name = lex_token.name;
                    lex_token = lex_string(lex_buffer, false);
                    if (lex_token.type == LexToken::StartParen)
                    {
                        auto function = lex_function(lex_buffer, variable);
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
                    else if (lex_token.type == LexToken::Assign)
                    {
                        auto out = recurse_var(variable, 0);

                        auto error = lex_expr(lex_buffer, true);
                        if (error.error) break;
                        auto expr = error.content;
                        auto expr_out = recurse_expr(*expr, 0);

                        auto var_name = buffer_string_ptr(variable.name);
                        printf("%s %s = %s", out.c_str(), std::string(var_name.content, var_name.size).c_str(), expr_out.c_str());
                    }
                    else
                    {
                        print_expectation_error(lex_buffer, lex_token, {"(", "="});
                        return 1;
                    }

                }
                else
                {
                    print_expectation_error(lex_buffer, lex_token, {"name"});
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
                else if (lex_token.keyword == Keyword::While)
                {
                    auto error = lex_expr(lex_buffer, true);
                    if (error.error) break;
                    auto expr = error.content;
                    auto out = recurse_expr(*expr, 0);
                    printf("while (%s) {}\n", out.c_str());
                }
            } 
            else if (lex_token.type == LexToken::SLComment)
            {
                lex_buffer.string = next_line(lex_buffer.string);
            }
            else
            {
                DebugLog(L"se fudeu");
                return 1;
            }
        }
    }
}