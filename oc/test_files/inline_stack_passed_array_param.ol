/**
* Author: Jack Robbins
* Test the ability of the system to handle a stack passed array param - both stores and loads
* when we have an inlined function that is using it. This is a very contrived case but
* it illustrates a point
*/


pub inline fn array_as_stack_param(x:i32, y:i32, z:i32, a:char, b:char, c:char, arr:mut i32[5]) -> i32 {
	if(a + b + c > 0){
		arr[1] = 5;
	} else {
		arr[2] = 7;
	}

	ret x + y + z + arr[a];
}


pub fn main() -> i32 {
	let x:mut i32[5] = [1, 2, 3, 4, 5];

	OUNIT: [exit_status = 17]
	ret @array_as_stack_param(1, 5, 6, 1, 1, 2, x);
}
