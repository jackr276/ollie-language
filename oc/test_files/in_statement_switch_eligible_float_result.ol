/**
 * Author: Jack Robbins
 * Test an in statement that is switch eligible, where we have a result type that is not a switch
 */


pub fn in_statement_float_result(x:i32) -> f32 {
	let result:f32 = x in (1, 2, 3, 4, 5);

	ret result;
}


pub fn main() -> i32 {
	OUNIT: [console = 1]
	ret @in_statement_float_result(3);
}
