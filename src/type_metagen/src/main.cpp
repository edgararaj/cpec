#include <stdio.h>

#include "../../file_utils.cpp"
#include "../../utils.h"

int wmain(int argc, const wchar_t** argv)
{
    if (argc < 5)
    {
        auto bold = "\x1b[1m";
        auto clear = "\x1b[0m";
        printf("Usage: %stype_metagen%s -o file -t types...", bold, clear);
        return 0;
    }

    Buffer buffer;
	buffer.size = KB(10);
	buffer.content = (char *)VirtualAlloc(0, buffer.size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    auto buffer_ptr = buffer.content;

    strcpy_s(buffer_ptr, buffer.size, "enum VarType { ");
    for (auto i = 4; i < argc; i++)
    {
        *(buffer_ptr + strlen(buffer_ptr)) = argv[i][0] - 32;
        auto size_needed = WideCharToMultiByte(CP_UTF8, 0, &argv[i][1], wcslen(&argv[i][1]), 0, 0, 0, 0);
	    auto utf8 = (char *)VirtualAlloc(0, size_needed, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        WideCharToMultiByte(CP_UTF8, 0, &argv[i][1], wcslen(&argv[i][1]), utf8, size_needed, 0, 0);
        strcpy_s(buffer_ptr + strlen(buffer_ptr), buffer.size, utf8);
        VirtualFree(utf8, size_needed, MEM_RELEASE);

        if (i < argc - 1)
        {
            strcpy_s(buffer_ptr + strlen(buffer_ptr), buffer.size, ", ");
        }
    }
    strcpy_s(buffer_ptr + strlen(buffer_ptr), buffer.size, " };\n");

    strcpy_s(buffer_ptr + strlen(buffer_ptr), buffer.size, "const char* var_types[] = { ");
    for (auto i = 4; i < argc; i++)
    {
        buffer_ptr += strcpy_s(buffer_ptr + strlen(buffer_ptr), buffer.size, "\"");
        auto size_needed = WideCharToMultiByte(CP_UTF8, 0, argv[i], wcslen(argv[i]), 0, 0, 0, 0);
	    auto utf8 = (char *)VirtualAlloc(0, size_needed, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        WideCharToMultiByte(CP_UTF8, 0, argv[i], wcslen(argv[i]), utf8, size_needed, 0, 0);
        strcpy_s(buffer_ptr + strlen(buffer_ptr), buffer.size, utf8);
        VirtualFree(utf8, size_needed, MEM_RELEASE);
        strcpy_s(buffer_ptr + strlen(buffer_ptr), buffer.size, "\"");

        if (i < argc - 1)
        {
            strcpy_s(buffer_ptr + strlen(buffer_ptr), buffer.size, ", ");
        }
    }
    strcpy_s(buffer_ptr + strlen(buffer_ptr), buffer.size, " };\n");

    auto out_file = create_wo_file(argv[2]);
    buffer.size = strlen(buffer_ptr);
    return write_file(out_file, argv[2], buffer);
}