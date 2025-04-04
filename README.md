# ollie-language
[![Lexer Test](https://github.com/jackr276/ollie-language/actions/workflows/lexer_CI.yml/badge.svg)](https://github.com/jackr276/ollie-language/actions/workflows/lexer_CI.yml)
[![Symbol table test](https://github.com/jackr276/ollie-language/actions/workflows/symtab_test.yml/badge.svg)](https://github.com/jackr276/ollie-language/actions/workflows/symtab_test.yml)
[![Parser test](https://github.com/jackr276/ollie-language/actions/workflows/parser_test.yml/badge.svg)](https://github.com/jackr276/ollie-language/actions/workflows/parser_test.yml)
[![Global compiler test](https://github.com/jackr276/ollie-language/actions/workflows/compiler_test.yml/badge.svg)](https://github.com/jackr276/ollie-language/actions/workflows/compiler_test.yml)

## Ollie Language Front-End

Conceptual Roadmap:
The Ollie Compiler takes a normal 3-phase approach for compilation

Phase 1: "Front-End"
The Front-End of the compiler is concerned with translating the source-code into an optimization-ready intermediate representation, called Ollie Intermediate Representation(OIR). The OIR will be given in SSA form, such that dataflow analysis is simplified. The general
flow is given below in the diagram

### Ollie Compiler Front-End

![Ollie Compiler Front End drawio(1)](https://github.com/user-attachments/assets/83e07b13-429a-47ef-ae84-7a831496d903)
