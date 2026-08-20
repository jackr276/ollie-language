/**
 * Author: Jack Robbins
 * Test a case where we copy from an external function(parameter pointer) inside
 * of an inlined function
 */


define struct my_struct {
	x:i32;
	y:i64;
	z:f64;
	c:char;
};

inline fn copy_from_param(origin:struct my_struct*, choice:bool) -> i32 {
	//Copy assignment
	let x:struct my_struct = *origin;

	if(choice) {
		ret x:x;
	} else {
		ret x:c;
	}
}


pub fn main() -> i32 {
	let origin:struct my_struct = {1, 2, 3, 'a'};

	OUNIT: [exit_status = 'a']
	ret @copy_from_param(&origin, false);
}
