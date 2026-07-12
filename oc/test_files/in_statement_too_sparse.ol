/**
 * Author: Jack Robbins
 * Test a case where we have an in-statement that's values are too sparse to be considered
 * for a switch, and will instead be turned into a chained conditional move
 */

 pub fn in_statement_too_sparse(x:i32) -> i32 {
 	let result:mut i32 = 5;

	//Too sparse, will be an if
	if(x in (5, 50, 500, 5000)){
		result += x;
	} else {
		result -= x;
	}

	ret result;
 }


 pub fn main() -> i32 {
 	//Should return 4 + 55 = 59
	OUNIT: [exit_status = 59]
 	ret @in_statement_too_sparse(1) + @in_statement_too_sparse(50);
 }
