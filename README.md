# ollie-language
[![Lexer Test](https://github.com/jackr276/ollie-language/actions/workflows/lexer_CI.yml/badge.svg)](https://github.com/jackr276/ollie-language/actions/workflows/lexer_CI.yml)
[![Symbol table test](https://github.com/jackr276/ollie-language/actions/workflows/symtab_test.yml/badge.svg)](https://github.com/jackr276/ollie-language/actions/workflows/symtab_test.yml)
[![Parser test](https://github.com/jackr276/ollie-language/actions/workflows/parser_test.yml/badge.svg)](https://github.com/jackr276/ollie-language/actions/workflows/parser_test.yml)
[![Global compiler test](https://github.com/jackr276/ollie-language/actions/workflows/compiler_test.yml/badge.svg)](https://github.com/jackr276/ollie-language/actions/workflows/compiler_test.yml)

Syntax of ollie-lang in BNF Form
Everything surrounded in angle brackets(\<>) is a nonterminal

1. \<program>::= {\<declaration-partition>}*

2. \<declaration-partition>::= \<function-definition>
                          	| \<declaration>

3. \<function-declaration>::= \<storage_spec>? func \<identifier> \<parameter-list> -> \<type-specifier> \<compound-statement>

4. \<storage-specifier> ::= static
	 			         | external
						 | register
						 | defined

5.  \<type-specifier> ::= {\<pointer>}* \<type>

6.  \<pointer> ::= * {const}* {\<pointer>}?

7.  \<type> ::= void 
			 | u_int8 
			 | s_int8 
			 | u_int16 
			 | s_int16 
			 | u_int32 
			 | s_int32 
			 | u_int64 
			 | s_int64
			 | float32 
			 | float64 
			 | char 
			 | str 
			 | \<enumeration-specifier> 
			 | \<structure-specifier> 
			 | \<user-defined-type>

8. 	\<structure-specifier> ::= structure \<identifier>{ {struct-declaration}+ }
							| structure { {struct-declaration}+ }
							| structure \<identifier>

9. \<structure-declaration> ::= {\<specifier-qualifier>}* \<structure-declarator-list>

10. \<specifier-qualifier> ::= \<type-specifier>
							| constant

11. \<structure-declarator-list> ::= \<structure-declarator>
								  | \<structure-declarator-list>, \<structure-declarator>

12. \<structure-declarator> ::= \<declarator>
							 | \<declarator> := \<constant-expression>
							 | := \<constant-expression>

13. \<declarator> ::= {\<pointer>}? \<direct-declarator>

14. \<direct-declarator> ::= \<identifier>
						  | ( \<declarator> )
						  | \<direct-declarator> [ {constant-expression}? ]
						  | \<direct-declarator> ( \<parameter-type-list> ) 
						  | \<direct-declarator> ( {\<identifier>}* )

15. \<constant-expression> ::= \<conditional-expression>

16. \<conditional-expression> ::= \<logical-or-expression>

17. \<logical-or-expression> ::= \<logical-and-expression> 
							  | \<logical-or-expression> || \<logical-and-expression>

18. \<logical-and-expression> ::= \<inclusive-or-expression> 
							   | \<logical-and-expression> && \<inclusive-or-expression>

19. \<inclusive-or-expression> ::= \<exclusive-or-expression> 
								| \<inclusive-or-expression> | \<exclusive-or-expression>

20. \<exclusive-or-expression> ::= \<and-expression> 
								| \<exclusive-or-expression> ^ \<and_expression>

21. \<and-expression> ::= \<equality-expression> 
					   | \<and-expression> & \<equality-expression>

22. \<equality-expression> ::= \<relational-expression> 
					   		| \<equality-expression> == \<relational-expression>
							| \<equality-expression> != \<relational-expression>

23. \<relational-expression> ::= \<shift-expression> 
							  | \<relational-expression> \< \<shift-expression> 
							  | \<relational-expression> > \<shift-expression> 
							  | \<relational-expression> >= \<shift-expression> 
							  | \<relational-expression> \<= \<shift-expression>

24. \<shift-expression> ::= \<additive-expression> 
				   		 | \<shift-expression> \<\< \<additive-expression> 
						 | \<shift-expression> >> \<additive-expression>

25. \<additive-expression> ::= \<multiplicative-expression> 
					  		| \<additive_expression> + \<multiplicative-expression>
					  		| \<additive-expression> - \<multiplicative-expression>

26. \<multiplicative-expression> ::= \<cast-expression>
								  | \<multiplicative-expression> * \<cast-expression>
								  | \<multiplicative-expression> / \<cast-expr>
								  | \<multiplicative-expression> % \<cast-expr>

27. \<cast-expression> ::= \<unary-expression> 
						| ( \<type-name> ) \<unary-expression>

28. \<unary-expression> ::= \<postfix-expression>
						 | ++\<unary-expression> 
						 | --\<unary-expression> 
						 | \<unary-operator> \<cast-expression>
						 | size \<unary-expression> 
						 | size \<type-name>

29. \<postfix-expression> ::= \<primary-expression>
						   | \<postfix-expression> [ \<expression> ]
						   | \<postfix-expression> ( {assignment-expression}* )
						   | \<postfix-expression> : \<identifier>
						   | \<postfix-expression> :: \<identifier>
						   | \<postfix-expression> ++
						   | \<postfix-expression> --

30. \<primary-expression> ::= \<identifier>
							| \<constant>
							| (\<expression>)

31. \<expression> ::= \<assignment-expression>
				   | \<expression, \<assignment-expression>

32. \<assignment-expression> ::= \<conditional-expression>
							 | \<unary-expression> \<assignment-operator> \<assignment-expression>

33. \<constant> ::= \<integer-constant>
				 | \<string-constant>
				 | \<float-constant>
				 | \<boolean-constant>

34. \<unary-operator> ::= & 
					  | * 
					  | ` 
					  | + 
					  | - 
					  | ~ 
					  | ! 

35. \<assignment-operator> ::= =
							| *=
							| /=
							| %=
							| +=
							| -=
							| \<\<=
							| >>=
							| &=
							| |=

36. \<type-name> ::= {\<specifier-qualifier>}+ {abstract-declarator}?

37. \<parameter-list> ::= \<parameter-declaration>
					   | \<parameter-list>, \<parameter-declaration>

38. \<parameter-declaration> ::= \<pointer>
							  | \<pointer> \<direct-abstract-declarator>
							  | \<direct-abstract-declarator>

39. \<direct-abstract-declarator> ::= ( \<abstract-declarator> )
								   | {\<direct-abstract-declarator>}? [ {\<constant-expression>}? ]
								   | {\<direct-abstract-declarator>}? ( {\<parameter-type-list>}?)

40. \<enumator-specifier> ::= enumerated \<identifier> { \<enumerator-list> }
						   | enumerated \<identifier>

41. \<enumerator-list> ::= \<enumerator>
						| \<enumator-list>, \<enumerator>

42. \<enumerator> ::= \<identifier>
				   | \<identifier> = \<constant-expression>

43. \<user-defined-type> ::= \<identifier>

44. \<declaration> ::= {\<declaration-specifier>}+ {\<initial-declarator>}* ;

45. \<initial-declarator> ::= \<declarator>
						  | \<declarator> = \<initializer>

46. \<initializer> ::= \<assignment-expression>
					| { \<initializer-list> }
					| { \<initializer-list>, }

47. \<initializer-list> ::= \<initializer>
						 | \<initializer-list> , \<initializer>

48. \<statement> ::= \<labeled-statement>
				  | \<expression-statement>
				  | \<compound-statement>
				  | \<if-statement>
				  | \<switch-statement>
				  | \<iterative-statement>
				  | \<jump-statement>

49. \<iterative-statement> ::= while( \<expression> ) do \<statement>
							| do \<statement> while( \<expression> )
							| for( {\<expression>}? ; {\<expression>}? ; {\<expression>}? ) do \<statement>
							
50. \<compound-statement> ::= {{\<declaration>}* {\<statement>}*}

51. \<if-statement> ::= if( \<expression> ) then \<statement> {else \<statement>}*

52. \<switch-statement> ::= switch on( \<expression> ) \<statement>

53. \<jump-statement> ::= jump \<identifier>
					     | continue on \<expression>
						 | continue;
						 | break on \<expression>
						 | break;
						 | ret {\<expression>}?

54. \<labeled-statement> ::= \<label-identifier> : \<statement>
						  | case \<constant-expression> : \<statement>
						  | defualt : \<statement>

55. \<expression-statement> ::= {\<expression>}?;

56. \<identifier> ::= (\<letter> | \<digit> | _ | $){(\<letter> | \<digit> | _ )}*

57. \<label-identifier> ::= ${(\<letter> | \<digit> | _ )}*

58. \<integer-constant>	::= {-}?[1-9][0-9]*

59. \<float-constnt> ::= {-}?[0-9]+.[1-9]*

60. \<string-constant>	 ::= "\<ASCII CHARACTER 33-127>"

61. \<boolean-constant> ::= (True | False)

62. \<digit>	::= [0-9]

63. \<letter> ::= [a-zA-Z]
