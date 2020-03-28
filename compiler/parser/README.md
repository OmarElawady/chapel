# Parser

On this branch, there exists two parsers, bison and antlr. To use antlr on the command line arguments add the option --use-antlr to the comiler. It currently parser simple arithmetic expressions and identifiers like `writeln(1 + 2**3);`.

There's a dependency on the antlr runtime library so it must be compiled from [source](https://www.antlr.org/download.html) and added to a linker-aware directory like /usr/local/lib.
