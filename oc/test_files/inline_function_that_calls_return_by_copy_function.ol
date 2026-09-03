/**
 * Author: Jack Robbins
 * Test the ability for us to have an inlined function that calls a return by copy function
 * and all of the stack handling that goes into that
 */

define struct my_struct {
	x:mut i32;
	y:mut i32;
	c:char[10];
};


fn build_struct(x:i32, y:i32) -> struct my_struct {
	let return_struct:struct my_struct = {x, y, ['h', 'i', ' ', 't', 'h', 'e', 'r', 'e', '!', '\0']};

	//Copy happens here
	ret return_struct;
}


inline fn call_ret_by_copy(x:i32, y:i32, choice:bool) -> i32 {
	//First build it
	let retval:struct my_struct = @build_struct(5, 4);

	//Another copy happening here
	let local_copy:struct my_struct = retval;

	if(choice) { 
		local_copy:x = x;
	} else {
		local_copy:x = y;
	}
	
	//Verify that we can get a difference
	ret retval:x + local_copy:x;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 16]
	ret @call_ret_by_copy(11, 17, true);
}
