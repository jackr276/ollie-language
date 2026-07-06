/**
* Author: Jack Robbins
* Test an if assignment that uses an in. If all is correct than this should trigger a conditional move conversion
*/


pub fn in_statement_assignment_conditional(x:u32) -> u32 {
	declare result:mut u32;

	if(x in (1211U, 1232U, 1200U, 1233U, 1222U, 1199U)){
		result = 5U;
	} else {
		result = 6U;
	}

	ret result;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 6]
	ret @in_statement_assignment_conditional(1234);
}
