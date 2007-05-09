#define EXTERN
#include "chpl.h"
#include "arg.h"
#include "countTokens.h"
#include "driver.h"
#include "files.h"
#include "misc.h"
#include "mysystem.h"
#include "runpasses.h"
#include "stmt.h"
#include "stringutil.h"
#include "version.h"
#include "log.h"
#include "primitive.h"


static void setChapelDebug(ArgumentState* arg_state, char* arg_unused);
static void printVersionAndExit(ArgumentState *arg_state, char *arg_unused);
static void printHelpAndExit(ArgumentState *arg_state, char *arg_unused);
static void printCopyrightAndExit(ArgumentState *arg_state, char *arg_unused);
static void printLicenseAndExit(ArgumentState* arg_state, char* arg_unused);
static void handleLibrary(ArgumentState* arg_state, char* arg_unused);
static void handleLibPath(ArgumentState* arg_state, char* arg_unused);
static void readConfigParam(ArgumentState* arg_state, char* arg_unused);

FILE* html_index_file = NULL;

int fdump_html = 0;
static char libraryFilename[FILENAME_MAX] = "";
static char log_flags[512] = "";
static bool rungdb = false;
bool no_codegen = false;
int debugParserLevel = 0;
bool developer = false;
bool ignore_errors = false;
int trace_level = 0;
int fcg = 0;
bool fDisableCopyPropagation = false;
bool fScalarReplaceTuples = false;
bool fDisableScalarReplaceArrayWrappers = false;
bool fDisableSingleLoopIteratorOpt = false;
bool fDisableExpandIteratorsInlineOpt = false;
bool fDisableLiveVariablesIteratorClassOpt = false;
bool unoptimized = false;
bool no_bounds_checking = false;
bool fRefCount = false;
bool no_inline = false;
bool report_inlining = false;
char chplhome[FILENAME_MAX] = ".";
char fExplainCall[256] = "";
char fPrintStatistics[256] = "";
bool fPrintDispatch = false;
bool fWarnPromotion = false;
bool copyCollect = false;
int fnostdincs = 0;
int num_constants_per_variable = 1;
char defaultDistribution[256] = "SingleLocaleDistribution";
int instantiation_limit = 256;
bool parallelPass = true;
char configParamString[FILENAME_MAX] = "";
Map<char*, char*> configParamMap;
bool debugCCode = false;
bool optimizeCCode = false;

static ArgumentDescription arg_desc[] = {
 {"", ' ', "Compilation Traces", NULL, NULL, NULL, NULL},
 {"print-passes", ' ', "Print compiler passes", "F", &printPasses, "CHPL_PRINT_PASSES", NULL},
 {"print-commands", ' ', "Print system commands", "F", &printSystemCommands, "CHPL_PRINT_COMMANDS", NULL},

 {"", ' ', "Code Size Statistics", NULL, NULL, NULL, NULL},
 {"print-code-size", ' ', "Print code size statistics", "F", &printTokens, "CHPL_PRINT_TOKENS", NULL},
 {"count-tokens", ' ', "Count tokens", "F", &countTokens, "CHPL_COUNT_TOKENS", NULL},

 {"", ' ', "Optimization Control", NULL, NULL, NULL, NULL},
 {"no-bounds-checks", ' ', "Disable bounds checking", "F", &no_bounds_checking, "CHPL_NO_BOUNDS_CHECKING", NULL},
 {"ref-count", ' ', "Enabled reference counting", "F", &fRefCount, "CHPL_REF_COUNT", NULL},
 {"no-inline", ' ', "Do not inline functions" , "F", &no_inline, NULL, NULL},
 {"unoptimized", ' ', "Generate naive code (many temps)", "F", &unoptimized, "CHPL_UNOPTIMIZED", NULL},
 {"scalar-replace-tuples", ' ', "Generate scalar components of tuples", "F", &fScalarReplaceTuples, "CHPL_SCALAR_REPLACE_TUPLES", NULL},
 {"no-single-loop-iterator-opt", ' ', "Disable the optimization of iterators composed of a single loop", "F", &fDisableSingleLoopIteratorOpt, "CHPL_DISABLE_SINGLE_LOOP_ITERATOR_OPT", NULL},
 {"no-expand-iterators-inline-opt", ' ', "Disable the expansion of iterators inlined around loop bodies", "F", &fDisableExpandIteratorsInlineOpt, "CHPL_DISABLE_EXPAND_ITERATORS_INLINE_OPT", NULL},
 {"no-copy-propagation", ' ', "Disable copy propagation", "F", &fDisableCopyPropagation, "CHPL_DISABLE_COPY_PROPAGATION", NULL},

 {"", ' ', "Code Generation", NULL, NULL, NULL, NULL},
 {"savec", ' ', "Save generated C code in directory", "P", saveCDir, "CHPL_SAVEC_DIR", NULL},
 {"debug", 'g', "Allow debugging of generated C code", "N", &debugCCode, "CHPL_DEBUG", setChapelDebug},
 {"optimize", 'O', "Optimize generated C code", "N", &optimizeCCode, "CHPL_OPTIMIZE", NULL},
 {"cg-cpp-lines", ' ', "Generate #line annotations", "N", &printCppLineno, "CHPL_CG_CPP_LINES", NULL},
 {"parallel", 'p', "Toggle threaded code generation", "T", &parallelPass, "CHPL_PARALLELIZE", NULL},

 {"", ' ', "Linker Control", NULL, NULL, NULL, NULL},
 {"output", 'o', "Name output executable", "P", executableFilename, "CHPL_EXE_NAME", NULL},
 {"lib-search-path", 'L', "C library search path", "P", libraryFilename, "CHPL_LIB_PATH", handleLibPath},
 {"lib-linkage", 'l', "C library linkage", "P", libraryFilename, "CHPL_LIB_NAME", handleLibrary},
 {"ccflags", ' ', "Back-end C compiler flags", "S256", ccflags, "CHPL_CC_FLAGS", NULL},

 {"", ' ', "Miscellaneous Flags", NULL, NULL, NULL, NULL},
 {"instantiate-max", ' ', "Limit number of instantiations", "I", &instantiation_limit, "CHPL_INSTANTIATION_LIMIT", NULL},
 {"chplhome", ' ', "Over-ride $CHPL_HOME", "P", chplhome, "CHPL_HOME", NULL},
 {"devel", ' ', "Compile as developer", "N", &developer, "CHPL_DEVELOPER", NULL},
 {"set", 's', "Set config param value", "S", configParamString, NULL, readConfigParam},
 {"explain-call", ' ', "Explain resolution of call", "S256", fExplainCall, NULL, NULL},

 {"", ' ', "Compiler Information", NULL, NULL, NULL, NULL},
 {"version", ' ', "Show Version", NULL, NULL, NULL, printVersionAndExit},
 {"copyright", ' ', "Show Copyright", NULL, NULL, NULL, printCopyrightAndExit},
 {"license", ' ', "Show License", NULL, NULL, NULL, printLicenseAndExit},
 {"help", 'h', "Help (show this list)", NULL, NULL, NULL, printHelpAndExit},

 {"", ' ', "Developer Flags", NULL, NULL, NULL, NULL},
 {"", ' ', "Debug Output", NULL, NULL, NULL, NULL},
 {"ccwarnings", ' ', "Give warnings for generated code", "T", &ccwarnings, "CHPL_CC_WARNINGS", NULL},
 {"html", 't', "Dump IR in HTML", "T", &fdump_html, "CHPL_HTML", NULL},
 {"print-statistics", ' ', "Print AST statistics", "S256", fPrintStatistics, NULL, NULL},
 {"print-dispatch", ' ', "Print dynamic dispatch table", "F", &fPrintDispatch, NULL, NULL},
 {"parser-debug", 'D', "Set parser debug level", "+", &debugParserLevel, "CHPL_PARSER_DEBUG", NULL},
 {"report-inlining", ' ', "Print inlined functions", "F", &report_inlining, NULL, NULL},
 {"log", 'd', "Specify debug logs", "S512", log_flags, "CHPL_LOG_FLAGS", log_flags_arg},
 {"log-dir", ' ', "Specify log directory", "P", log_dir, "CHPL_LOG_DIR", NULL},

 {"", ' ', "Misc. Developer Flags", NULL, NULL, NULL, NULL},
 {"gdb", ' ', "Run compiler in gdb", "F", &rungdb, NULL, NULL},
 {"nostdincs", ' ', "Don't use standard modules", "T", &fnostdincs, "CHPL_NOSTDINCS", NULL},
 {"no-codegen", ' ', "Suppress code generation", "F", &no_codegen, "CHPL_NO_CODEGEN", NULL},
 {"ignore-errors", ' ', "Attempt to ignore errors", "F", &ignore_errors, "CHPL_IGNORE_ERRORS", NULL},
 {"warn-promotion", ' ', "Warn about scalar promotion", "F", &fWarnPromotion, NULL, NULL},
 {"default-dist", ' ', "Change the default distribution", "S256", defaultDistribution, "CHPL_DEFAULT_DISTRIBUTION", NULL},
 {"no-scalar-replace-array-wrappers", ' ', "Generate explicit array wrappers", "F", &fDisableScalarReplaceArrayWrappers, "CHPL_DISABLE_SCALAR_REPLACE_ARRAY_WRAPPERS", NULL},
 {"no-live-variables-iterator-class-opt", ' ', "Do not use live variable analysis for iterator class construction", "F", &fDisableLiveVariablesIteratorClassOpt, "CHPL_DISABLE_LIVE_VARIABLE_ITERATOR_CLASS_OPT", NULL},
 {"copy-collect", ' ', "Turn on copying garbage-collection", "F", &copyCollect, NULL, NULL},
 {0}
};


static ArgumentState arg_state = {
  0, 0,
  "program", 
  "path",
  arg_desc
};

static void printLicenseAndExit(ArgumentState *arg_state, char *arg_unused) {
  fprintf(stderr,
#include "LICENSE"
          );
  clean_exit(0);
}

static void printCopyright(void) {
  fprintf(stderr,
#include "COPYRIGHT"
          );
}


static void printCopyrightAndExit(ArgumentState* arg_state, char* arg_unused) {
  printCopyright();
  clean_exit(0);
}

static void printVersion(ArgumentState* arg_state) {
  char ver[30];
  get_version(ver);
  fprintf(stderr, "%s Version %s\n", arg_state->program_name, ver);  
  printCopyright();
}

static void setChapelDebug(ArgumentState* arg_state, char* arg_unused) {
  printCppLineno = true;
}

static void printVersionAndExit(ArgumentState* arg_state, char* arg_unused) {
  printVersion(arg_state);
  clean_exit(0);
}

static void printHelpAndExit(ArgumentState *arg_state, char *arg_unused) {
  usage(arg_state, arg_unused);
  clean_exit(0);
}

static void 
handleLibrary(ArgumentState* arg_state, char* arg_unused) {
  addLibInfo(stringcat("-l", libraryFilename));
}

static void 
handleLibPath(ArgumentState* arg_state, char* arg_unused) {
  addLibInfo(stringcat("-L", libraryFilename));
}

static void
compile_all(void) {
  initPrimitive();
  initPrimitiveTypes();
  testInputFiles(arg_state.nfile_arguments, arg_state.file_argument);
  runPasses();
}

static void
compute_program_name_loc(char* orig_argv0, char** name, char** loc) {
  char* argv0 = stringcpy(orig_argv0);
  char* lastslash = strrchr(argv0, '/');
  if (lastslash == NULL) {
    *name = argv0;
    *loc = ".";   // BLC: this is inaccurate; we should search the path.  
                  // It's no less accurate than what we did previously, though.
  } else {
    *lastslash = '\0';
    *name = lastslash+1;
    *loc = argv0;
  }
  snprintf(chplhome, FILENAME_MAX, "%s", stringcat(*loc, "/../.."));
}


void runCompilerInGDB(int argc, char* argv[]) {
  char* gdbCommandFilename = createGDBFile(argc, argv);
  char* command = stringcat("gdb -q ", argv[0]," -x ", gdbCommandFilename);
  int status = mysystem(command, "running gdb", 0);

  clean_exit(status);
}


static void readConfigParam(ArgumentState* arg_state, char* arg_unused) {
  // Expect arg_unused to b a string of either of these forms:
  // 1. name=value -- set the config param "name" to "value"
  // 2. name       -- set the boolean config param "name" to NOT("name")
  //                  if name is not type bool, set it to 0.

  char *name = stringcpy(arg_unused);
  char *value;
  value = strstr(name, "=");
  if (value) {
    *value = '\0';
    value++;
    if (value[0]) {
      // arg_unused was name=value
      configParamMap.put(canonicalize_string(name), value);
    } else {
      // arg_unused was name=  <blank>
      USR_FATAL("Missing config param value");
    }
  } else {
    // arg_unused was just name
    configParamMap.put(canonicalize_string(name), "");
  }
}


int
main(int argc, char *argv[]) {
  compute_program_name_loc(argv[0], &(arg_state.program_name),
                           &(arg_state.program_loc));
  process_args(&arg_state, argc, argv);
  startCatchingSignals();
  if (arg_state.nfile_arguments < 1)
    printHelpAndExit(&arg_state, NULL);
  if (rungdb)
    runCompilerInGDB(argc, argv);
  if (fdump_html || strcmp(log_flags, ""))
    init_logs();
  compile_all();
  free_args(&arg_state);
  clean_exit(0);
  return 0;
}
