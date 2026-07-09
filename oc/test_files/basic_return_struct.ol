/**
* Author: Jack Robbins
* Test the most basic case where we are returning a struct
*/


//Very basic struct that we are using to return
define struct my_struct {
	x:i32;
	y:i64;
	z:char;
} as custom_struct;



pub fn return_struct() -> custom_struct {
	let return_value:custom_struct = {3, 4, 'a'};

	ret return_value;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 'a']
	ret @return_struct():z;
}
