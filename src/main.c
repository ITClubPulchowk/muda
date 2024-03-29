﻿
#include "cmd_line.h"
#include "lenstring.h"
#include "muda_parser.h"
#include "os.h"
#include "stream.h"
#include "zBase.h"

#if PLATFORM_OS_WINDOWS == 1
const char *ExecutableExtension     = "exe";
const char *StaticLibraryExtension  = "lib";
const char *DynamicLibraryExtension = "dll";
#elif PLATFORM_OS_LINUX == 1
const char *ExecutableExtension     = "out";
const char *StaticLibraryExtension  = "a";
const char *DynamicLibraryExtension = "so";
#else
#error "Unimplemented"
#endif

void MudaParseSectionInit(Muda_Parse_Section *section)
{
    section->OS       = Muda_Parsing_OS_All;
    section->Compiler = Muda_Parsing_COMPILER_ALL;
}

void DeserializeMuda(Build_Config *build_config, Compiler_Config_List *config_list, Uint8 *data, Compiler_Kind compiler,
                     const char *parent)
{
    Memory_Arena         *scratch = ThreadScratchpad();

    Compiler_Config      *config  = CompilerConfigListAdd(config_list, StringLiteral("default"));

    Muda_Parser           prsr    = MudaParseInit(data, config->Arena);

    Uint32                version = 0;
    Uint32                major = 0, minor = 0, patch = 0;

    Muda_Parsing_COMPILER selected_compiler;
    if (compiler == Compiler_Bit_CL)
        selected_compiler = Muda_Parsing_COMPILER_CL;
    else if (compiler == Compiler_Bit_CLANG)
        selected_compiler = Muda_Parsing_COMPILER_CLANG;
    else
        selected_compiler = Muda_Parsing_COMPILER_GCC;

    while (MudaParseNext(&prsr))
        if (prsr.Token.Kind != Muda_Token_Comment)
            break;

    if (prsr.Token.Kind == Muda_Token_Tag &&
        StrMatchCaseInsensitive(prsr.Token.Data.Tag.Title, StringLiteral("version")))
    {
        if (prsr.Token.Data.Tag.Value.Data)
        {
            if (sscanf(prsr.Token.Data.Tag.Value.Data, "%d.%d.%d", &major, &minor, &patch) != 3)
                FatalError("Error: Bad file version\n");
        }
        else
            FatalError("Error: Version info missing\n");
    }
    else
        FatalError("Error: Version tag missing at top of file\n");

    version = MudaMakeVersion(major, minor, patch);

    if (version < MUDA_BACKWARDS_COMPATIBLE_VERSION || version > MUDA_CURRENT_VERSION)
    {
        String error = FmtStr(scratch,
                              "Version %u.%u.%u not supported. \n"
                              "Minimum version supported: %u.%u.%u\n"
                              "Current version: %u.%u.%u\n\n",
                              major, minor, patch, MUDA_BACKWARDS_COMPATIBLE_VERSION_MAJOR,
                              MUDA_BACKWARDS_COMPATIBLE_VERSION_MINOR, MUDA_BACKWARDS_COMPATIBLE_VERSION_PATCH,
                              MUDA_VERSION_MAJOR, MUDA_VERSION_MINOR, MUDA_VERSION_PATCH);
        FatalError(error.Data);
    }

    Muda_Parse_Section section;
    MudaParseSectionInit(&section);

    bool first_config_name = true;

    while (MudaParseNext(&prsr))
    {
        switch (prsr.Token.Kind)
        {
        case Muda_Token_Config: {
            MudaParseSectionInit(&section);

            if (first_config_name)
            {
                config->Name      = StrDuplicateArena(prsr.Token.Data.Config, config->Arena);
                first_config_name = false;
            }
            else
            {
                config = CompilerConfigListFindOrAdd(config_list, prsr.Token.Data.Config);
            }
        }
        break;

        case Muda_Token_Section: {
            if (StrMatchCaseInsensitive(StringLiteral("OS.ALL"), prsr.Token.Data.Section))
            {
                section.OS       = Muda_Parsing_OS_All;
                section.Compiler = Muda_Parsing_COMPILER_ALL;
            }
            else if (StrMatchCaseInsensitive(StringLiteral("OS.WINDOWS"), prsr.Token.Data.Section))
            {
                section.OS       = Muda_Parsing_OS_Windows;
                section.Compiler = Muda_Parsing_COMPILER_ALL;
            }
            else if (StrMatchCaseInsensitive(StringLiteral("OS.LINUX"), prsr.Token.Data.Section))
            {
                section.OS       = Muda_Parsing_OS_Linux;
                section.Compiler = Muda_Parsing_COMPILER_ALL;
            }
            else if (StrMatchCaseInsensitive(StringLiteral("OS.MAC"), prsr.Token.Data.Section))
            {
                section.OS       = Muda_Parsing_OS_Mac;
                section.Compiler = Muda_Parsing_COMPILER_ALL;
            }
            else if (StrMatchCaseInsensitive(StringLiteral("COMPILER.ALL"), prsr.Token.Data.Section))
            {
                section.Compiler = Muda_Parsing_COMPILER_ALL;
                section.OS       = Muda_Parsing_OS_All;
            }
            else if (StrMatchCaseInsensitive(StringLiteral("COMPILER.CL"), prsr.Token.Data.Section))
            {
                section.Compiler = Muda_Parsing_COMPILER_CL;
                section.OS       = Muda_Parsing_OS_All;
            }
            else if (StrMatchCaseInsensitive(StringLiteral("COMPILER.CLANG"), prsr.Token.Data.Section))
            {
                section.Compiler = Muda_Parsing_COMPILER_CLANG;
                section.OS       = Muda_Parsing_OS_All;
            }
            else if (StrMatchCaseInsensitive(StringLiteral("COMPILER.GCC"), prsr.Token.Data.Section))
            {
                section.Compiler = Muda_Parsing_COMPILER_GCC;
                section.OS       = Muda_Parsing_OS_All;
            }
            else if (StrMatchCaseInsensitive(StringLiteral("OS.WINDOWS.CL"), prsr.Token.Data.Section))
            {
                section.Compiler = Muda_Parsing_COMPILER_CL;
                section.OS       = Muda_Parsing_OS_Windows;
            }
            else if (StrMatchCaseInsensitive(StringLiteral("OS.WINDOWS.CLANG"), prsr.Token.Data.Section))
            {
                section.Compiler = Muda_Parsing_COMPILER_CLANG;
                section.OS       = Muda_Parsing_OS_Windows;
            }
            else if (StrMatchCaseInsensitive(StringLiteral("OS.WINDOWS.GCC"), prsr.Token.Data.Section))
            {
                section.Compiler = Muda_Parsing_COMPILER_GCC;
                section.OS       = Muda_Parsing_OS_Windows;
            }
            else if (StrMatchCaseInsensitive(StringLiteral("OS.LINUX.CL"), prsr.Token.Data.Section))
            {
                section.Compiler = Muda_Parsing_COMPILER_CL;
                section.OS       = Muda_Parsing_OS_Linux;
            }
            else if (StrMatchCaseInsensitive(StringLiteral("OS.LINUX.CLANG"), prsr.Token.Data.Section))
            {
                section.Compiler = Muda_Parsing_COMPILER_CLANG;
                section.OS       = Muda_Parsing_OS_Linux;
            }
            else if (StrMatchCaseInsensitive(StringLiteral("OS.LINUX.GCC"), prsr.Token.Data.Section))
            {
                section.Compiler = Muda_Parsing_COMPILER_GCC;
                section.OS       = Muda_Parsing_OS_Linux;
            }
            else if (StrMatchCaseInsensitive(StringLiteral("OS.MAC.CL"), prsr.Token.Data.Section))
            {
                section.Compiler = Muda_Parsing_COMPILER_CL;
                section.OS       = Muda_Parsing_OS_Mac;
            }
            else if (StrMatchCaseInsensitive(StringLiteral("OS.MAC.CLANG"), prsr.Token.Data.Section))
            {
                section.Compiler = Muda_Parsing_COMPILER_CLANG;
                section.OS       = Muda_Parsing_OS_Mac;
            }
            else if (StrMatchCaseInsensitive(StringLiteral("OS.MAC.GCC"), prsr.Token.Data.Section))
            {
                section.Compiler = Muda_Parsing_COMPILER_GCC;
                section.OS       = Muda_Parsing_OS_Mac;
            }
            else
            {
                String error = FmtStr(scratch, "Line: %u, Column: %u :: Unknown Section: %s\n", prsr.line, prsr.column,
                                      prsr.Token.Data.Section.Data);
                FatalError(error.Data);
            }
        }
        break;

        case Muda_Token_Property: {
            first_config_name = false;

            bool reject_os =
                !(section.OS == Muda_Parsing_OS_All || (PLATFORM_OS_WINDOWS && section.OS == Muda_Parsing_OS_Windows) ||
                  (PLATFORM_OS_LINUX && section.OS == Muda_Parsing_OS_Linux) ||
                  (PLATFORM_OS_MAC && section.OS == Muda_Parsing_OS_Mac));

            if (reject_os || (section.Compiler != Muda_Parsing_COMPILER_ALL && selected_compiler != section.Compiler))
                break;

            bool        property_found = false;

            Muda_Token *token          = &prsr.Token;

            if (token->Data.Property.Count != 0)
            {
                for (Uint32 index = 0; index < ArrayCount(CompilerConfigMemberTypeInfo); ++index)
                {
                    const Compiler_Config_Member *const info = &CompilerConfigMemberTypeInfo[index];
                    if (StrMatch(info->Name, token->Data.Property.Key))
                    {
                        property_found = true;

                        switch (info->Kind)
                        {
                        case Compiler_Config_Member_Enum: {
                            Enum_Info *en    = (Enum_Info *)info->KindInfo;

                            bool       found = false;
                            for (Uint32 index = 0; index < en->Count; ++index)
                            {
                                if (StrMatch(en->Ids[index], *token->Data.Property.Value))
                                {
                                    Uint32 *in = (Uint32 *)((char *)config + info->Offset);
                                    *in        = index;
                                    found      = true;
                                    break;
                                }
                            }

                            if (!found)
                            {
                                Out_Stream       out;
                                Memory_Allocator allocator = MemoryArenaAllocator(scratch);
                                OutCreate(&out, allocator);

                                Temporary_Memory temp = BeginTemporaryMemory(scratch);

                                for (Uint32 index = 0; index < en->Count; ++index)
                                {
                                    OutString(&out, en->Ids[index]);
                                    OutBuffer(&out, ", ", 2);
                                }

                                String accepted_values = OutBuildString(&out, &allocator);

                                LogWarn("Line: %u, Column: %u :: Invalid value for Property \"%s\" : %s. Acceptable "
                                        "values are: %s\n",
                                        prsr.line, prsr.column, info->Name.Data, token->Data.Property.Value->Data,
                                        accepted_values);

                                EndTemporaryMemory(&temp);
                            }
                        }
                        break;

                        case Compiler_Config_Member_Bool: {
                            bool *in = (bool *)((char *)config + info->Offset);

                            if (StrMatch(StringLiteral("1"), *token->Data.Property.Value) ||
                                StrMatch(StringLiteral("True"), *token->Data.Property.Value))
                            {
                                *in = true;
                            }
                            else if (StrMatch(StringLiteral("0"), *token->Data.Property.Value) ||
                                     StrMatch(StringLiteral("False"), *token->Data.Property.Value))
                            {
                                *in = false;
                            }
                            else
                            {
                                String error = FmtStr(scratch, "Line: %u, Column: %u :: Expected boolean: %s\n",
                                                      prsr.line, prsr.column, prsr.Token.Data);
                                FatalError(error.Data);
                            }
                        }
                        break;

                        case Compiler_Config_Member_String: {
                            if (token->Data.Property.Count == 1)
                            {
                                String *in = (String *)((char *)config + info->Offset);
                                *in        = token->Data.Property.Value[0];
                            }
                            else
                            {
                                String error =
                                    FmtStr(scratch, "Line: %u, Column: %u :: %s property only accepts single value\n",
                                           prsr.line, prsr.column, info->Name.Data);
                                FatalError(error.Data);
                            }
                        }
                        break;

                        case Compiler_Config_Member_String_Array: {
                            String_Array_List *in = (String_Array_List *)((char *)config + info->Offset);
                            StringArrayListAdd(in, prsr.Token.Data.Property.Value, prsr.Token.Data.Property.Count,
                                               config->Arena);
                        }
                        break;

                            NoDefaultCase();
                        }

                        break;
                    }
                }
            }
            else
                property_found = true;

            if (!property_found)
            {
                Muda_Plugin_Event pevent;
                pevent.Kind                   = Muda_Plugin_Event_Kind_Parse;
                pevent.Data.Parse.Section     = section;
                pevent.Data.Parse.Key.Data    = token->Data.Property.Key.Data;
                pevent.Data.Parse.Key.Length  = token->Data.Property.Key.Length;
                pevent.Data.Parse.ConfigName  = config->Name.Data;
                pevent.Data.Parse.MudaDirName = parent;
                pevent.Data.Parse.Values      = (Muda_String *)token->Data.Property.Value;
                pevent.Data.Parse.ValueCount  = (uint32_t)token->Data.Property.Count;

                if (build_config->PluginHook(&ThreadContext, &build_config->Interface, &pevent) != 0)
                {
                    LogWarn("Line: %u, Column: %u :: Invalid Property \"%s\". Ignored.\n", prsr.line, prsr.column,
                            token->Data.Property.Key.Data);
                }
            }
        }
        break;

        case Muda_Token_Comment: {
            // ignored
        }
        break;

        case Muda_Token_Tag: {
            LogWarn("Line: %u, Column: %u :: Tag %s not supported. Ignored.\n", prsr.line, prsr.column,
                    prsr.Token.Data.Tag.Title.Data);
        }
        break;
        }
    }

    if (prsr.Token.Kind == Muda_Token_Error)
    {
        String errmsg = FmtStr(scratch, "%s at Line %d, Column %d\n", prsr.Token.Data.Error.Desc,
                               prsr.Token.Data.Error.Line, prsr.Token.Data.Error.Column);
        FatalError(errmsg.Data);
    }
}

const char *GetCompilerName(Compiler_Kind kind)
{
    if (kind == Compiler_Bit_CL)
        return "CL";
    else if (kind == Compiler_Bit_CLANG)
        return "CLANG";
    else if (kind == Compiler_Bit_GCC)
        return "GCC";
    Unreachable();
    return "";
}

typedef struct Directory_Iteration_Context
{
    Memory_Arena      *Arena;
    String_List       *List;
    String_Array_List *Ignore;
} Directory_Iteration_Context;

static Directory_Iteration DirectoryIteratorAddToList(const File_Info *info, void *user_context)
{
    if ((info->Atribute & File_Attribute_Directory) && !(info->Atribute & File_Attribute_Hidden))
    {
        if (StrMatch(info->Name, StringLiteral(".muda")))
            return Directory_Iteration_Continue;

        Directory_Iteration_Context *context  = (Directory_Iteration_Context *)user_context;

        String_Array_List           *ignore   = context->Ignore;
        String                       dir_path = info->Path;
        String                       dir_name = SubStr(dir_path, 2, dir_path.Length - 2);
        ForList(String_Array_List_Node, ignore)
        {
            ForListNode(ignore, MAX_STRING_NODE_DATA_COUNT)
            {
                Int64 str_count = it->Data[index].Count;
                for (Int64 str_index = 0; str_index < str_count; ++str_index)
                {
#if PLATFORM_OS_WINDOWS == 1
                    if (StrMatchCaseInsensitive(dir_name, it->Data[index].Values[str_index]) ||
                        StrMatchCaseInsensitive(dir_path, it->Data[index].Values[str_index]))
                        return Directory_Iteration_Continue;
#else
                    if (StrMatch(dir_name, it->Data[index].Values[str_index]) ||
                        StrMatch(dir_path, it->Data[index].Values[str_index]))
                        return Directory_Iteration_Continue;
#endif
                }
            }
        }

        StringListAdd(context->List, StrDuplicateArena(info->Path, context->Arena), context->Arena);
    }
    return Directory_Iteration_Continue;
}

void ExecuteMudaBuild(Compiler_Config *compiler_config, Build_Config *build_config,
                      const Compiler_Kind available_compilers, const Compiler_Kind compiler, const char *parent,
                      bool is_root);
void SearchExecuteMudaBuild(Memory_Arena *arena, Build_Config *build_config, const Compiler_Kind available_compilers,
                            const Compiler_Kind compiler, Compiler_Config *alternative_config, const char *parent,
                            bool is_root);

void ExecuteMudaBuild(Compiler_Config *compiler_config, Build_Config *build_config,
                      const Compiler_Kind available_compilers, const Compiler_Kind compiler, const char *parent,
                      bool is_root)
{
    Memory_Arena    *scratch       = ThreadScratchpad();

    Temporary_Memory temp          = BeginTemporaryMemory(scratch);

    bool             prebuild_pass = true;
    if (compiler_config->Prebuild.Length)
    {
        LogInfo("==> Executing Prebuild command\n");
        if (!OsExecuteCommandLine(compiler_config->Prebuild))
        {
            prebuild_pass = false;
            LogError("Prebuild execution failed. Aborted.\n\n");
            if (compiler_config->Kind != Compile_Project)
                return;
        }
        else
        {
            LogInfo("Finished executing Prebuild command\n");
        }
    }

    bool              execute_postbuild = true;

    Muda_Plugin_Event pevent;
    memset(&pevent, 0, sizeof(pevent));

    if (compiler_config->Kind == Compile_Project)
    {
        String build_dir                 = compiler_config->BuildDirectory;
        String build                     = compiler_config->Build;

        pevent.Data.Prebuild.MudaDirName = parent;
        pevent.Data.Prebuild.Name        = compiler_config->Name.Data;
        pevent.Data.Prebuild.Build       = build.Data;
        pevent.Data.Prebuild.BuildDir    = build_dir.Data;
        pevent.Data.Prebuild.Succeeded   = prebuild_pass;
        pevent.Data.Prebuild.RootBuild   = is_root;

        if (compiler_config->Application == Application_Executable)
            pevent.Data.Prebuild.BuildExtension = ExecutableExtension;
        else if (compiler_config->Application == Application_Dynamic_Library)
            pevent.Data.Prebuild.BuildExtension = DynamicLibraryExtension;
        else
            pevent.Data.Prebuild.BuildExtension = StaticLibraryExtension;

        pevent.Kind = Muda_Plugin_Event_Kind_Prebuild;
        build_config->PluginHook(&ThreadContext, &build_config->Interface, &pevent);

        if (!prebuild_pass)
            return;

        LogInfo("Beginning compilation\n");

        Out_Stream out;
        OutCreate(&out, MemoryArenaAllocator(compiler_config->Arena));

        Out_Stream lib;
        OutCreate(&lib, MemoryArenaAllocator(compiler_config->Arena));

        Out_Stream res;
        OutCreate(&res, MemoryArenaAllocator(compiler_config->Arena));

        Uint32 result = OsCheckIfPathExists(build_dir);
        if (result == Path_Does_Not_Exist)
        {
            if (!OsCreateDirectoryRecursively(build_dir))
            {
                LogError("Failed to create directory %s! Aborted.\n", build_dir.Data);
                return;
            }
        }
        else if (result == Path_Exist_File)
        {
            LogError("%s: Path exist but is a file! Aborted.\n", build_dir.Data);
            return;
        }

        if (compiler == Compiler_Bit_CL)
        {
            // For CL, we output intermediate files to "BuildDirectory/int"
            String intermediate;
            if (build_dir.Data[build_dir.Length - 1] == '/')
                intermediate = FmtStr(scratch, "%sint", build_dir.Data);
            else
                intermediate = FmtStr(scratch, "%s/int", build_dir.Data);

            result = OsCheckIfPathExists(intermediate);
            if (result == Path_Does_Not_Exist)
            {
                if (!OsCreateDirectoryRecursively(intermediate))
                {
                    LogError("Failed to create directory %s! Aborted.\n", intermediate.Data);
                    return;
                }
            }
            else if (result == Path_Exist_File)
            {
                LogError("%s: Path exist but is a file! Aborted.\n", intermediate.Data);
                return;
            }
        }

        // Turn on Optimization if it is forced via command line
        if (build_config->ForceOptimization)
        {
            LogInfo("Optimization turned on forcefully\n");
            compiler_config->Optimization = true;
        }

        switch (compiler)
        {
        case Compiler_Bit_CL: {
            OutFormatted(&out, "cl -nologo -EHsc -W3 ");
            OutFormatted(&out, "%s ", compiler_config->Optimization ? "-O2" : "-Od");

            if (compiler_config->DebugSymbol)
            {
                OutFormatted(&out, "-Zi ");
            }

            ForList(String_Array_List_Node, &compiler_config->Defines)
            {
                ForListNode(&compiler_config->Defines, MAX_STRING_NODE_DATA_COUNT)
                {
                    Int64 str_count = it->Data[index].Count;
                    for (Int64 str_index = 0; str_index < str_count; ++str_index)
                        OutFormatted(&out, "-D%s ", it->Data[index].Values[str_index].Data);
                }
            }

            ForList(String_Array_List_Node, &compiler_config->IncludeDirectories)
            {
                ForListNode(&compiler_config->IncludeDirectories, MAX_STRING_NODE_DATA_COUNT)
                {
                    Int64 str_count = it->Data[index].Count;
                    for (Int64 str_index = 0; str_index < str_count; ++str_index)
                        OutFormatted(&out, "-I\"%s\" ", it->Data[index].Values[str_index].Data);
                }
            }

            ForList(String_Array_List_Node, &compiler_config->Sources)
            {
                ForListNode(&compiler_config->Sources, MAX_STRING_NODE_DATA_COUNT)
                {
                    Int64 str_count = it->Data[index].Count;
                    for (Int64 str_index = 0; str_index < str_count; ++str_index)
                        OutFormatted(&out, "\"%s\" ", it->Data[index].Values[str_index].Data);
                }
            }

#if PLATFORM_OS_WINDOWS == 1
            if (compiler_config->ResourceFile.Length)
            {
                OutFormatted(&res, "rc -fo \"%s/%s.res\" \"%s\" ", build_dir.Data, build.Data,
                             compiler_config->ResourceFile.Data);
                OutFormatted(&out, "\"%s/%s.res\" ", build_dir.Data, build.Data);
            }
#endif

            ForList(String_Array_List_Node, &compiler_config->Flags)
            {
                ForListNode(&compiler_config->Flags, MAX_STRING_NODE_DATA_COUNT)
                {
                    Int64 str_count = it->Data[index].Count;
                    for (Int64 str_index = 0; str_index < str_count; ++str_index)
                        OutFormatted(&out, "%s ", it->Data[index].Values[str_index].Data);
                }
            }

            OutFormatted(&out, "-Fd\"%s/\" ", build_dir.Data);

            if (compiler_config->Application != Application_Static_Library)
            {
                OutFormatted(&out, "-Fo\"%s/int/\" ", build_dir.Data);

                if (compiler_config->Application == Application_Dynamic_Library)
                    OutFormatted(&out, "-LD ");

                OutFormatted(&out, "-link ");
                OutFormatted(&out, "-pdb:\"%s/%s.pdb\" ", build_dir.Data, build.Data);

                if (compiler_config->Application != Application_Static_Library)
                    OutFormatted(&out, "-out:\"%s/%s.%s\" ", build_dir.Data, build.Data,
                                 compiler_config->Application == Application_Executable ? ExecutableExtension
                                                                                        : DynamicLibraryExtension);

                if (compiler_config->Application == Application_Dynamic_Library)
                    OutFormatted(&out, "-IMPLIB:\"%s/%s.%s\" ", build_dir.Data, build.Data, StaticLibraryExtension);

                ForList(String_Array_List_Node, &compiler_config->LinkerFlags)
                {
                    ForListNode(&compiler_config->LinkerFlags, MAX_STRING_NODE_DATA_COUNT)
                    {
                        Int64 str_count = it->Data[index].Count;
                        for (Int64 str_index = 0; str_index < str_count; ++str_index)
                            OutFormatted(&out, "%s ", it->Data[index].Values[str_index].Data);
                    }
                }
            }
            else
            {
                OutFormatted(&out, "-c ");
                OutFormatted(&out, "-Fo\"%s/int/%s.obj\" ", build_dir.Data, build.Data);

                OutFormatted(&lib, "lib -nologo \"%s/int/%s.obj\" ", build_dir.Data, build.Data);
                OutFormatted(&lib, "-out:\"%s/%s.%s\" ", build_dir.Data, build.Data, StaticLibraryExtension);
            }

            Out_Stream *target = ((compiler_config->Application != Application_Static_Library) ? &out : &lib);

            ForList(String_Array_List_Node, &compiler_config->LibraryDirectories)
            {
                ForListNode(&compiler_config->LibraryDirectories, MAX_STRING_NODE_DATA_COUNT)
                {
                    Int64 str_count = it->Data[index].Count;
                    for (Int64 str_index = 0; str_index < str_count; ++str_index)
                        OutFormatted(target, "-LIBPATH:\"%s\" ", it->Data[index].Values[str_index].Data);
                }
            }

            ForList(String_Array_List_Node, &compiler_config->Libraries)
            {
                ForListNode(&compiler_config->Libraries, MAX_STRING_NODE_DATA_COUNT)
                {
                    Int64 str_count = it->Data[index].Count;
                    for (Int64 str_index = 0; str_index < str_count; ++str_index)
                        OutFormatted(target, "\"%s.%s\" ", it->Data[index].Values[str_index].Data,
                                     StaticLibraryExtension);
                }
            }

            if (PLATFORM_OS_WINDOWS)
            {
                OutFormatted(target, "-SUBSYSTEM:%s ",
                             compiler_config->Subsystem == Subsystem_Console ? "CONSOLE" : "WINDOWS");
            }
        }
        break;

        case Compiler_Bit_CLANG: {
            OutFormatted(&out, "%s -Wall ", compiler_config->Language ? "clang++" : "clang");

            if (compiler_config->DebugSymbol)
            {
                OutFormatted(&out, "-g -gcodeview ");
            }

            OutFormatted(&out, "%s ", compiler_config->Optimization ? "--optimize" : "--debug");

            ForList(String_Array_List_Node, &compiler_config->Defines)
            {
                ForListNode(&compiler_config->Defines, MAX_STRING_NODE_DATA_COUNT)
                {
                    Int64 str_count = it->Data[index].Count;
                    for (Int64 str_index = 0; str_index < str_count; ++str_index)
                        OutFormatted(&out, "-D%s ", it->Data[index].Values[str_index].Data);
                }
            }

            ForList(String_Array_List_Node, &compiler_config->IncludeDirectories)
            {
                ForListNode(&compiler_config->IncludeDirectories, MAX_STRING_NODE_DATA_COUNT)
                {
                    Int64 str_count = it->Data[index].Count;
                    for (Int64 str_index = 0; str_index < str_count; ++str_index)
                        OutFormatted(&out, "-I\"%s\" ", it->Data[index].Values[str_index].Data);
                }
            }

            ForList(String_Array_List_Node, &compiler_config->Sources)
            {
                ForListNode(&compiler_config->Sources, MAX_STRING_NODE_DATA_COUNT)
                {
                    Int64 str_count = it->Data[index].Count;
                    for (Int64 str_index = 0; str_index < str_count; ++str_index)
                        OutFormatted(&out, "\"%s\" ", it->Data[index].Values[str_index].Data);
                }
            }

#if PLATFORM_OS_WINDOWS == 1
            if (compiler_config->ResourceFile.Length)
            {
                OutFormatted(&res, "llvm-rc -FO \"%s/%s.res\" \"%s\" ", build_dir.Data, build.Data,
                             compiler_config->ResourceFile.Data);
                OutFormatted(&out, "\"%s/%s.res\" ", build_dir.Data, build.Data);
            }
#endif

            ForList(String_Array_List_Node, &compiler_config->Flags)
            {
                ForListNode(&compiler_config->Flags, MAX_STRING_NODE_DATA_COUNT)
                {
                    Int64 str_count = it->Data[index].Count;
                    for (Int64 str_index = 0; str_index < str_count; ++str_index)
                        OutFormatted(&out, "%s ", it->Data[index].Values[str_index].Data);
                }
            }

            if (compiler_config->Application != Application_Static_Library)
            {

                if (compiler_config->Application == Application_Dynamic_Library)
                    OutFormatted(&out, "--shared ");

                if (compiler_config->Application != Application_Static_Library)
                {
                    OutFormatted(&out, "-o \"%s/%s.%s\" ", build_dir.Data, build.Data,
                                 compiler_config->Application == Application_Executable ? ExecutableExtension
                                                                                        : DynamicLibraryExtension);
                }

                ForList(String_Array_List_Node, &compiler_config->LinkerFlags)
                {
                    ForListNode(&compiler_config->LinkerFlags, MAX_STRING_NODE_DATA_COUNT)
                    {
                        Int64 str_count = it->Data[index].Count;
                        for (Int64 str_index = 0; str_index < str_count; ++str_index)
                            OutFormatted(&out, "%s ", it->Data[index].Values[str_index].Data);
                    }
                }
            }
            else
            {
                OutFormatted(&out, "-c ");
                OutFormatted(&lib, "-o \"%s/%s.%s\" ", build_dir.Data, build.Data, StaticLibraryExtension);
            }

            Out_Stream *target = ((compiler_config->Application != Application_Static_Library) ? &out : &lib);

            ForList(String_Array_List_Node, &compiler_config->LibraryDirectories)
            {
                ForListNode(&compiler_config->LibraryDirectories, MAX_STRING_NODE_DATA_COUNT)
                {
                    Int64 str_count = it->Data[index].Count;
                    for (Int64 str_index = 0; str_index < str_count; ++str_index)
                        OutFormatted(target, "-L\"%s\" ", it->Data[index].Values[str_index].Data);
                }
            }

            ForList(String_Array_List_Node, &compiler_config->Libraries)
            {
                ForListNode(&compiler_config->Libraries, MAX_STRING_NODE_DATA_COUNT)
                {
                    Int64 str_count = it->Data[index].Count;
                    for (Int64 str_index = 0; str_index < str_count; ++str_index)
                        OutFormatted(target, "\"-l%s\" ", it->Data[index].Values[str_index].Data);
                }
            }

            if (PLATFORM_OS_WINDOWS)
            {
                if (available_compilers & Compiler_Bit_GCC)
                {
                    OutFormatted(target, "-fuse-ld=ld ");
                    OutFormatted(target, "\"-Wl,--subsystem,%s\" ",
                                 compiler_config->Subsystem == Subsystem_Console ? "console" : "windows");
                }
                else
                {
                    if (available_compilers & Compiler_Bit_CL)
                        OutFormatted(target, "-fuse-ld=link ");
                    else
                        OutFormatted(target, "-fuse-ld=lld ");
                    OutFormatted(target, "-Xlinker -subsystem:%s ",
                                 compiler_config->Subsystem == Subsystem_Console ? "CONSOLE" : "WINDOWS");
                }
            }
        }
        break;

        case Compiler_Bit_GCC: {
            OutFormatted(&out, "%s -Wall ", compiler_config->Language ? "g++" : "gcc");
            OutFormatted(&out, "%s ", compiler_config->Optimization ? "-O2" : "-O");

            ForList(String_Array_List_Node, &compiler_config->Defines)
            {
                ForListNode(&compiler_config->Defines, MAX_STRING_NODE_DATA_COUNT)
                {
                    Int64 str_count = it->Data[index].Count;
                    for (Int64 str_index = 0; str_index < str_count; ++str_index)
                        OutFormatted(&out, "-D%s ", it->Data[index].Values[str_index].Data);
                }
            }

            ForList(String_Array_List_Node, &compiler_config->IncludeDirectories)
            {
                ForListNode(&compiler_config->IncludeDirectories, MAX_STRING_NODE_DATA_COUNT)
                {
                    Int64 str_count = it->Data[index].Count;
                    for (Int64 str_index = 0; str_index < str_count; ++str_index)
                        OutFormatted(&out, "-I\"%s\" ", it->Data[index].Values[str_index].Data);
                }
            }

            ForList(String_Array_List_Node, &compiler_config->Sources)
            {
                ForListNode(&compiler_config->Sources, MAX_STRING_NODE_DATA_COUNT)
                {
                    Int64 str_count = it->Data[index].Count;
                    for (Int64 str_index = 0; str_index < str_count; ++str_index)
                        OutFormatted(&out, "\"%s\" ", it->Data[index].Values[str_index].Data);
                }
            }

#if PLATFORM_OS_WINDOWS == 1
            if (compiler_config->ResourceFile.Length)
            {
                OutFormatted(&res, "windres -i \"%s\" \"%s/%s.o\" ", compiler_config->ResourceFile.Data, build_dir.Data,
                             build.Data);
                OutFormatted(&out, "\"%s/%s.o\" ", build_dir.Data, build.Data);
            }
#endif

            ForList(String_Array_List_Node, &compiler_config->Flags)
            {
                ForListNode(&compiler_config->Flags, MAX_STRING_NODE_DATA_COUNT)
                {
                    Int64 str_count = it->Data[index].Count;
                    for (Int64 str_index = 0; str_index < str_count; ++str_index)
                        OutFormatted(&out, "%s ", it->Data[index].Values[str_index].Data);
                }
            }

            if (compiler_config->Application != Application_Static_Library)
            {

                if (compiler_config->Application == Application_Dynamic_Library)
                    OutFormatted(&out, "--shared ");

                if (compiler_config->Application != Application_Static_Library)
                    OutFormatted(&out, "-o \"%s/%s.%s\" ", build_dir.Data, build.Data,
                                 compiler_config->Application == Application_Executable ? ExecutableExtension
                                                                                        : DynamicLibraryExtension);

                ForList(String_Array_List_Node, &compiler_config->LinkerFlags)
                {
                    ForListNode(&compiler_config->LinkerFlags, MAX_STRING_NODE_DATA_COUNT)
                    {
                        Int64 str_count = it->Data[index].Count;
                        for (Int64 str_index = 0; str_index < str_count; ++str_index)
                            OutFormatted(&out, "%s ", it->Data[index].Values[str_index].Data);
                    }
                }
            }
            else
            {
                OutFormatted(&out, "-c ");
                OutFormatted(&lib, "-o \"%s/%s.%s\" ", build_dir.Data, build.Data, StaticLibraryExtension);
            }

            Out_Stream *target = ((compiler_config->Application != Application_Static_Library) ? &out : &lib);

            ForList(String_Array_List_Node, &compiler_config->LibraryDirectories)
            {
                ForListNode(&compiler_config->LibraryDirectories, MAX_STRING_NODE_DATA_COUNT)
                {
                    Int64 str_count = it->Data[index].Count;
                    for (Int64 str_index = 0; str_index < str_count; ++str_index)
                        OutFormatted(target, "-L\"%s\" ", it->Data[index].Values[str_index].Data);
                }
            }

            ForList(String_Array_List_Node, &compiler_config->Libraries)
            {
                ForListNode(&compiler_config->Libraries, MAX_STRING_NODE_DATA_COUNT)
                {
                    Int64 str_count = it->Data[index].Count;
                    for (Int64 str_index = 0; str_index < str_count; ++str_index)
                        OutFormatted(target, "\"-l%s\" ", it->Data[index].Values[str_index].Data);
                }
            }

            if (PLATFORM_OS_WINDOWS)
            {
                OutFormatted(target, "-w -Wl,-subsystem,%s ",
                             compiler_config->Subsystem == Subsystem_Console ? "console" : "windows");
            }
        }
        break;
        }

        String cmd_line                  = OutBuildStringSerial(&out, compiler_config->Arena);

        execute_postbuild                = false;

        bool resource_compilation_passed = true;
        if (res.Size)
        {
            LogInfo("Executing Resource compilation\n");
            String resource_cmd_line = OutBuildStringSerial(&res, scratch);

            if (build_config->DisplayCommandLine)
            {
                LogInfo("Resource Command Line: %s\n", resource_cmd_line.Data);
            }

            if (OsExecuteCommandLine(resource_cmd_line))
            {
                LogInfo("Resource Compilation succeeded\n");
            }
            else
            {
                LogInfo("Resource Compilation failed\n");
                resource_compilation_passed = false;
            }
        }

        if (resource_compilation_passed)
        {
            if (build_config->DisplayCommandLine)
            {
                LogInfo("Compiler Command Line: %s\n", cmd_line.Data);
            }

            LogInfo("Executing compilation\n");
            if (OsExecuteCommandLine(cmd_line))
            {
                LogInfo("Compilation succeeded\n\n");
                if (lib.Size)
                {
                    if (build_config->DisplayCommandLine)
                    {
                        LogInfo("Linker Command Line: %s\n", cmd_line.Data);
                    }

                    LogInfo("Creating static library\n");
                    cmd_line = OutBuildStringSerial(&lib, compiler_config->Arena);
                    if (OsExecuteCommandLine(cmd_line))
                    {
                        LogInfo("Library creation succeeded\n");
                        execute_postbuild = true;
                    }
                    else
                    {
                        LogError("Library creation failed\n");
                    }
                }
                else
                {
                    execute_postbuild = true;
                }
            }
            else
            {
                LogError("Compilation failed\n\n");
            }
        }
    }
    else
    {
        Assert(compiler_config->Kind == Compile_Solution);

        LogInfo("==> Found Solution Muda Build\n");

        compiler_config->Kind     = Compile_Project;

        Memory_Arena *dir_scratch = ThreadScratchpadI(1);

        String_List   directory_list;
        StringListInit(&directory_list);
        String_Array_List directory_array_list;
        StringArrayListInit(&directory_array_list);

        String_Array_List *filtered_list = &directory_array_list;

        if (StringArrayListIsEmpty(&compiler_config->ProjectDirectories))
        {
            Directory_Iteration_Context directory_iteration;
            directory_iteration.Arena  = dir_scratch;
            directory_iteration.List   = &directory_list;
            directory_iteration.Ignore = &compiler_config->IgnoredDirectories;
            OsIterateDirectory(".", DirectoryIteratorAddToList, &directory_iteration);

            ForList(String_List_Node, &directory_list)
            {
                StringArrayListAdd(filtered_list, it->Data, it->Next ? MAX_STRING_NODE_DATA_COUNT : directory_list.Used,
                                   dir_scratch);
            }
        }
        else
        {
            filtered_list = &compiler_config->ProjectDirectories;
        }

        Memory_Arena *arena = compiler_config->Arena;
        ForList(String_Array_List_Node, filtered_list)
        {
            ForListNode(filtered_list, MAX_STRING_NODE_DATA_COUNT)
            {
                Int64 str_count = it->Data[index].Count;
                for (Int64 str_index = 0; str_index < str_count; ++str_index)
                {
                    String *values = it->Data[index].Values;

                    LogInfo("==> Executing Muda Build in \"%s\" \n", values[str_index].Data);

                    if (OsSetWorkingDirectory(values[str_index]))
                    {
                        String parent_dir = values[str_index];
                        parent_dir.Data += 2;
                        parent_dir.Length -= 2;
                        Temporary_Memory arena_temp = BeginTemporaryMemory(arena);
                        SearchExecuteMudaBuild(arena, build_config, available_compilers, compiler, compiler_config,
                                               parent_dir.Data, false);
                        EndTemporaryMemory(&arena_temp);
                        if (!OsSetWorkingDirectory(StringLiteral("..")))
                        {
                            LogError("Could not set the original directory as the working directory! Aborted.\n");
                            return;
                        }
                    }
                    else
                    {
                        LogError("Could not set \"%s\" as working directory, skipped.", values[str_index].Data);
                    }
                }
            }
        }

        MemoryArenaReset(dir_scratch);

        compiler_config->Kind = Compile_Solution;
    }

    if (execute_postbuild && compiler_config->Postbuild.Length)
    {
        LogInfo("==> Executing Postbuild command\n");
        if (!OsExecuteCommandLine(compiler_config->Postbuild))
        {
            execute_postbuild = false;
            LogError("Postbuild execution failed. \n\n");
        }
        else
        {
            LogInfo("Finished executing Postbuild command\n");
        }
    }

    if (compiler_config->Kind == Compile_Project)
    {
        pevent.Kind                    = Muda_Plugin_Event_Kind_Postbuild;
        pevent.Data.Prebuild.Succeeded = execute_postbuild;
        build_config->PluginHook(&ThreadContext, &build_config->Interface, &pevent);
    }

    EndTemporaryMemory(&temp);
}

void SearchExecuteMudaBuild(Memory_Arena *arena, Build_Config *build_config, const Compiler_Kind available_compilers,
                            const Compiler_Kind compiler, Compiler_Config *alternative_config, const char *parent,
                            bool is_root)
{
    Temporary_Memory      arena_temp = BeginTemporaryMemory(arena);

    Compiler_Config_List *configs    = (Compiler_Config_List *)PushSize(arena, sizeof(Compiler_Config_List));
    CompilerConfigListInit(configs, arena);

    String       config_path   = {0, 0};

    const String LocalMudaFile = StringLiteral("build.muda");
    if (OsCheckIfPathExists(LocalMudaFile) == Path_Exist_File)
    {
        config_path = LocalMudaFile;
    }
    else if (alternative_config)
    {
        CompilerConfigListCopy(configs, alternative_config);
    }
    else
    {
        String muda_user_path = OsGetUserConfigurationPath(StringLiteral("muda/config.muda"));
        if (OsCheckIfPathExists(muda_user_path) == Path_Exist_File)
        {
            config_path = muda_user_path;
        }
    }

    if (config_path.Length)
    {
        LogInfo("Found muda configuration file: \"%s\"\n", config_path.Data);

        File_Handle fp = OsFileOpen(config_path, File_Mode_Read);
        if (fp.PlatformFileHandle)
        {
            Ptrsize       size                       = OsFileGetSize(fp);
            const Ptrsize MAX_ALLOWED_MUDA_FILE_SIZE = MegaBytes(32);

            if (size > MAX_ALLOWED_MUDA_FILE_SIZE)
            {
                float max_size = (float)MAX_ALLOWED_MUDA_FILE_SIZE / (1024 * 1024);
                LogError("File %s too large. Max memory: %.3fMB! Aborted.\n", config_path.Data, max_size);
                return;
            }

            Uint8 *buffer = PushSize(configs->Arena, size + 1);
            if (OsFileRead(fp, buffer, size) && size > 0)
            {
                buffer[size] = 0;
                LogInfo("Parsing muda file\n");
                DeserializeMuda(build_config, configs, buffer, compiler, parent);
                LogInfo("Finished parsing muda file\n");
            }
            else if (size == 0)
            {
                LogError("File %s is empty!\n", config_path.Data);
            }
            else
            {
                LogError("Could not read the configuration file %s!\n", config_path.Data);
            }

            OsFileClose(fp);
        }
        else
        {
            LogError("Could not open the configuration file %s!\n", config_path.Data);
        }
    }

    if (build_config->ConfigurationCount == 0)
    {
        ForList(Compiler_Config_Node, configs)
        {
            ForListNode(configs, ArrayCount(configs->Head.Config))
            {
                Compiler_Config *config = &it->Config[index];
                LogInfo("==> Building Configuration: %s \n", config->Name.Data);
                PushDefaultCompilerConfig(config, config->Kind == Compile_Project);
                ExecuteMudaBuild(config, build_config, available_compilers, compiler, parent, is_root);
            }
        }
    }
    else
    {
        for (Uint32 index = 0; index < build_config->ConfigurationCount; ++index)
        {
            Compiler_Config *config          = NULL;
            String           required_config = build_config->Configurations[index];

            ForList(Compiler_Config_Node, configs)
            {
                ForListNode(configs, ArrayCount(configs->Head.Config))
                {
                    if (StrMatch(it->Config[index].Name, required_config))
                    {
                        config = &it->Config[index];
                        break;
                    }
                }
            }

            if (config)
            {
                LogInfo("==> Building Configuration: %s \n", config->Name.Data);
                PushDefaultCompilerConfig(config, config->Kind == Compile_Project);
                ExecuteMudaBuild(config, build_config, available_compilers, compiler, parent, is_root);
            }
            else
            {
                LogError("Configuration \"%s\" not found. Ignored.\n", required_config.Data);
            }
        }
    }

    EndTemporaryMemory(&arena_temp);
}

int main(int argc, char *argv[])
{
    InitThreadContext(NullMemoryAllocator(), MegaBytes(512), (Log_Agent){.Procedure = LogProcedure},
                      FatalErrorProcedure);

    OsSetupConsole();

    Build_Config build_config;
    BuildConfigInit(&build_config);
    if (HandleCommandLineArguments(argc, argv, &build_config))
        return 0;

    Compiler_Kind compiler = OsDetectCompiler();
    if (compiler == 0)
    {
        LogError("Failed to detect compiler! Installation of compiler is required...\n");
        if (PLATFORM_OS_WINDOWS)
            OsConsoleWrite("[Visual Studio - MSVC] https://visualstudio.microsoft.com/ \n");
        OsConsoleWrite("[CLANG] https://releases.llvm.org/download.html \n");
        OsConsoleWrite("[GCC] https://gcc.gnu.org/install/download.html \n");
        return 1;
    }

    if (build_config.DisableLogs)
        ThreadContext.LogAgent.Procedure = LogProcedureDisabled;

    if (build_config.LogFilePath)
    {
        File_Handle handle =
            OsFileOpen(StringMake(build_config.LogFilePath, strlen(build_config.LogFilePath)), File_Mode_Write);
        if (handle.PlatformFileHandle)
        {
            ThreadContext.LogAgent.Data = handle.PlatformFileHandle;
            LogInfo("Logging to file: %s\n", build_config.LogFilePath);
        }
        else
        {
            LogError("Could not open file: \"%s\" for logging. Logging to file disabled.\n");
        }
    }

    void *plugin = NULL;
    if (build_config.EnablePlugins)
    {
        plugin = OsLibraryLoad(MudaPluginPath);
        if (plugin)
        {
            typedef void (*PluginVersionProc)(uint32_t * major, uint32_t * minor, uint32_t * patch);
            PluginVersionProc check_version = (PluginVersionProc)OsGetProcedureAddress(plugin, "MudaAcceptVersion");

            if (check_version)
            {
                uint32_t major, minor, patch;
                check_version(&major, &minor, &patch);

                uint32_t supported_major   = MUDA_PLUGIN_BACK_COMPATIBLE_VERSION_MAJOR;
                uint32_t supported_minor   = MUDA_PLUGIN_BACK_COMPATIBLE_VERSION_MINOR;
                uint32_t supported_patch   = MUDA_PLUGIN_BACK_COMPATIBLE_VERSION_PATCH;

                uint32_t plugin_version    = MudaMakeVersion(major, minor, patch);
                uint32_t supported_version = MudaMakeVersion(supported_major, supported_minor, supported_patch);
                uint32_t current_version   = MUDA_CURRENT_VERSION;

                if (plugin_version <= current_version && plugin_version >= supported_version)
                {
                    build_config.PluginHook =
                        (Muda_Event_Hook_Procedure)OsGetProcedureAddress(plugin, MudaPluginProcedureName);
                    if (build_config.PluginHook)
                    {
                        Muda_Parsing_COMPILER forced_compiler = Muda_Parsing_COMPILER_ALL;
                        if (build_config.ForceCompiler == Compiler_Bit_CL)
                            forced_compiler = Muda_Parsing_COMPILER_CL;
                        else if (build_config.ForceCompiler == Compiler_Bit_CLANG)
                            forced_compiler = Muda_Parsing_COMPILER_CLANG;
                        else if (build_config.ForceCompiler == Compiler_Bit_GCC)
                            forced_compiler = Muda_Parsing_COMPILER_GCC;

                        build_config.Interface.CommandLineConfig.Flags = 0;
                        if (build_config.ForceCompiler)
                            build_config.Interface.CommandLineConfig.Flags |= Command_Line_Flag_Force_Optimization;
                        if (build_config.DisplayCommandLine)
                            build_config.Interface.CommandLineConfig.Flags |= Command_Line_Flag_Display_Command_Line;
                        if (build_config.DisableLogs)
                            build_config.Interface.CommandLineConfig.Flags |= Command_Line_Flag_Disable_Logs;

                        build_config.Interface.CommandLineConfig.ForceCompiler      = forced_compiler;
                        build_config.Interface.CommandLineConfig.Configurations =
                            (Muda_String *)build_config.Configurations;
                        build_config.Interface.CommandLineConfig.ConfigurationCount = build_config.ConfigurationCount;
                        build_config.Interface.CommandLineConfig.LogFilePath        = build_config.LogFilePath;

                        Muda_Plugin_Event pevent;
                        memset(&pevent, 0, sizeof(pevent));
                        pevent.Kind = Muda_Plugin_Event_Kind_Detection;
                        if (build_config.PluginHook(&ThreadContext, &build_config.Interface, &pevent) == 0)
                        {
                            LogInfo("Plugin detected. Name: %s\n", build_config.Interface.PluginName);
                        }
                        else
                        {
                            LogInfo("Loading of Plugin failed.\n");
                            build_config.PluginHook = NullMudaEventHook;
                        }
                    }
                    else
                    {
                        LogWarn("Plugin dectected by could not be loaded\n");
                    }
                }
                else
                {
                    LogWarn("Plugin dectected but not supported. Plugin version: %u.%u.%u. Min Supported version: "
                            "%u.%u.%u. Current version: %u.%u.%u.",
                            major, minor, patch, supported_major, supported_minor, supported_patch, MUDA_VERSION_MAJOR,
                            MUDA_VERSION_MINOR, MUDA_VERSION_PATCH);
                }
            }
        }
    }

    Compiler_Kind available_compilers = compiler;

    if (build_config.ForceCompiler)
    {
        if (compiler & build_config.ForceCompiler)
        {
            compiler = build_config.ForceCompiler;
            LogInfo("Requested compiler: %s\n", GetCompilerName(compiler));
        }
        else
        {
            const char *requested_compiler = GetCompilerName(build_config.ForceCompiler);
            LogInfo("Requested compiler: %s but %s could not be detected\n", requested_compiler, requested_compiler);
        }
    }

    // Set one compiler from all available compilers
    // This priorities one compiler over the other
    if (compiler & Compiler_Bit_CL)
    {
        compiler = Compiler_Bit_CL;
        LogInfo("Compiler MSVC Detected.\n");
    }
    else if (compiler & Compiler_Bit_CLANG)
    {
        compiler = Compiler_Bit_CLANG;
        LogInfo("Compiler CLANG Detected.\n");
    }
    else
    {
        compiler = Compiler_Bit_GCC;
        LogInfo("Compiler GCC Detected.\n");
    }

    Memory_Arena arena            = MemoryArenaCreate(MegaBytes(128));

    const char  *current_dir_name = OsGetWorkingDirectoryName(&arena);
    SearchExecuteMudaBuild(&arena, &build_config, available_compilers, compiler, NULL, current_dir_name, true);

    Muda_Plugin_Event pevent;
    memset(&pevent, 0, sizeof(pevent));
    pevent.Kind = Muda_Plugin_Event_Kind_Destroy;
    build_config.PluginHook(&ThreadContext, &build_config.Interface, &pevent);

    if (ThreadContext.LogAgent.Data)
    {
        File_Handle handle;
        handle.PlatformFileHandle = ThreadContext.LogAgent.Data;
        OsFileClose(handle);
    }

    if (plugin)
    {
        OsLibraryFree(plugin);
    }

    return 0;
}
