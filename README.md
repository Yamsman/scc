# scc

A hand-written C compiler with the objectives of being C99 compliant and self-hosting.
Currently, a lot of detailed functionality is missing, but the compiler can already output limited x86\_64 assembly for some constructions.

## TODO
Major features that aren't implemented yet:

### Lexical analysis and preprocessing
* User-defined type recognition

### Parsing
* Switch statements
* Initializers
* Bitfields
* Typedefs
* Type casting

### Code generation
* Integer expressions
* Floating-point expressions
* Declarations
* Referencing
* Array access/Dereferencing
* Function calls

### Miscellaneous
* Restructure AST nodes
* Possibly change instruction nodes to be an intermediate representation?
* Optimization
