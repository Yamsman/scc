# scc

A C compiler written with the objective of supporting most C99 features in addition to being self-hosting.
Currently missing a good amount of functionality, but can currently output a limited amount of x86_64 assembly.

## TODO

### Lexical analysis and preprocessing
* Function macro expansion
* User-defined type recognition

### Parsing
* For loops
* Switch statements
* Ternary operator
* Enums
* Initializers
* Bitfields
* Typedefs
* Macro expansion in constant expressions

### Code generation
* Integer expressions
* Floating-point expressions
* Declarations
* Referencing
* Array access
* Function calls

### Miscellaneous
* Restructure AST nodes
* Possibly change instruction nodes to be an intermediate representation?
* Optimization
