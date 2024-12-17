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


 my_struct ref struct_ptr

### Binary Operators:
 - All standard: +, -, /, *, %

