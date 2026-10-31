/**
 * Author: Jack Robbins
 * Test a very contrived case where we're returning by copy inside of a loop. In reality
 * this is very bad performance wise, but I want to make sure that the analysis can handle
 * this edge case
 */

define struct my_struct {
	x:mut i32;
	y:mut i32[4];
	z:mut char;
} as custom_struct;


pub fn return_struct_value(x:i32, y:i32) -> struct my_struct {
	ret {x, [y, y + 1, y + 2, y + 3], 'a'};
}


pub fn return_by_copy_in_loop(count:i32) -> i32 {
	let result:mut i32 = 0;

	for(let i:mut i32 = 0; i < count; i++){
		let returned:struct my_struct = @return_struct_value(i, i + 1);

		result += returned:y[1];
	}

	ret result;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 20]
	ret @return_by_copy_in_loop(5);
}
