/**
 * Author: Jack Robbins
 * Test a basic case where we inline with a stack param that is going to require
 * stack parameter setup and teardown
 */


define struct my_struct {
	x:i32[5];
	y:i32[5];
};


inline fn inline_with_stack(choice:bool, index:i32, target:struct my_struct) -> i32{
	if(choice){
		ret target:x[index];
	} else {
		ret target:y[index];
	}
}


pub fn main() -> i32 {
	let param:struct my_struct = {[1, 2, 3, 4, 5], [6, 7, 8, 9, 10]};

	OUNIT: [exit_status = 5]
	ret @inline_with_stack(true, 4, param);
}
