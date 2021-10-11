#include "lenstring.h"
#include "os.h"

typedef enum Compile_Type {
	Compile_Type_Project,
	Compile_Type_Solution,
} Compile_Type;

typedef struct Compiler_Config {
	Compile_Type Type;
	bool Optimization;
	String Build;
	String BuildDirectory;
	String_List Defines;
	String_List IncludeDirectory;
	String_List Source;
	String_List LibraryDirectory;
	String_List Library;
} Compiler_Config;

//
//
//

INLINE_PROCEDURE void CompilerConfigInit(Compiler_Config *config) {
    memset(config, 0, sizeof(*config));
    config->Defines.Tail = &config->Defines.Head;
    config->IncludeDirectory.Tail = &config->IncludeDirectory.Head;
    config->Source.Tail = &config->Source.Head;
    config->LibraryDirectory.Tail = &config->Source.Head;
    config->Library.Tail = &config->Source.Head;
}

INLINE_PROCEDURE void PushDefaultCompilerConfig(Compiler_Config *config, Compiler_Kind compiler) {
    if (config->BuildDirectory.Length == 0) {
        config->BuildDirectory = StringLiteral("./bin");
    }

    if (config->Build.Length == 0) {
        config->Build = StringLiteral("output");
    }

    if (StringListIsEmpty(&config->Source)) {
        StringListAdd(&config->Source, StringLiteral("*.c"));
    }

    if (compiler == Compiler_Kind_CL && StringListIsEmpty(&config->Defines)) {
        StringListAdd(&config->Defines, StringLiteral("_CRT_SECURE_NO_WARNINGS"));
    }
}

INLINE_PROCEDURE void PrintCompilerConfig(Compiler_Config conf) {
    OsConsoleWrite("\nType                : %s", conf.Type == Compile_Type_Project ? "Project" : "Solution");
    OsConsoleWrite("\nOptimization        : %s", conf.Optimization ? "True" : "False");
    OsConsoleWrite("\nBuild               : %s", conf.Build.Data);
    OsConsoleWrite("\nBuild Directory     : %s", conf.BuildDirectory.Data);

    OsConsoleWrite("\nSource              : ");
    for (String_List_Node *ntr = &conf.Source.Head; ntr && conf.Source.Used; ntr = ntr->Next) {
        int len = ntr->Next ? 8 : conf.Source.Used;
        for (int i = 0; i < len; i++) OsConsoleWrite("%s ", ntr->Data[i].Data);
    }
    OsConsoleWrite("\nDefines             : ");
    for (String_List_Node *ntr = &conf.Defines.Head; ntr && conf.Defines.Used; ntr = ntr->Next) {
        int len = ntr->Next ? 8 : conf.Defines.Used;
        for (int i = 0; i < len; i++) OsConsoleWrite("%s ", ntr->Data[i].Data);
    }
    OsConsoleWrite("\nInclude Directories : ");
    for (String_List_Node *ntr = &conf.IncludeDirectory.Head; ntr && conf.IncludeDirectory.Used; ntr = ntr->Next) {
        int len = ntr->Next ? 8 : conf.IncludeDirectory.Used;
        for (int i = 0; i < len; i++) OsConsoleWrite("%s ", ntr->Data[i].Data);
    }
    OsConsoleWrite("\nLibrary Directories : ");
    for (String_List_Node *ntr = &conf.LibraryDirectory.Head; ntr && conf.LibraryDirectory.Used; ntr = ntr->Next) {
        int len = ntr->Next ? 8 : conf.LibraryDirectory.Used;
        for (int i = 0; i < len; i++) OsConsoleWrite("%s ", ntr->Data[i].Data);
    }
    OsConsoleWrite("\nLibraries           : ");
    for (String_List_Node *ntr = &conf.Library.Head; ntr && conf.Library.Used; ntr = ntr->Next) {
        int len = ntr->Next ? 8 : conf.Library.Used;
        for (int i = 0; i < len; i++) OsConsoleWrite("%s ", ntr->Data[i].Data);
    }
    OsConsoleWrite("\n");
}
