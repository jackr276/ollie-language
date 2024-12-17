# ollie-language

**Basic Requirements**
- Statically typed
- Systems-level programming language
- Needs explicit syntax, no confusion with indirection
- Procedural(imperative) language, no explicit object-oriented programming
- Support for user-defined types & structures(maybe union types but these are rarely used) is needed
- User-enforced memory safety, not language enforced
- Main function is global entry point
- *Add more as needed*

<unary-operator><expression>


### Unary Operators:
 - memaddr (equivalent of &) 
 - deref (*var)
 - ref (char* -> char ref)
 - l_not logical not(!)
 - b_not bitwise not(~)
 - 

 int i = 0 

!i = 1
~i = -1


### Types
- s_int8, u_int8, s_int16, u_int16, s_int32, u_int32, s_int64, u_int64, float32, float64(double)
- char(u_int8)
- str(char*)(maybe)
- void

GNU Assembly AT&T syntax
movq %rax, %rbx "Move quad-word(8 bytes) from rax into rbx"


 my_struct ref struct_ptr

### Binary Operators:
 - All standard: +, -, /, *, %

