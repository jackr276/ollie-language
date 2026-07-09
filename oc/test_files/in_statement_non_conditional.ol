/**
* Author: Jack Robbins
* Test a basic ollie in statement when it is not being used in a conditional
*/


pub fn in_in_use(x:i32) -> i32 {
	let result:bool = x in (1, 3, 4, 7, 11);
	ret result;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 1]
	ret @in_in_use(7);
}
