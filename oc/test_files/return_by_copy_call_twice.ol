/**
* Author: Jack Robbins
* Test calling a return by copy style function more than once inside of another function. This
* should all work fine. This test is OUNIT compatible
*/

define struct my_struct {
	x:i32;
	y:i32[4];
	z:char;
} as custom_struct;


pub fn return_struct(x:i32, y:i32) -> custom_struct {
	let return_value:custom_struct = {x, [y, 1, 2, 3], 'a'};

	ret return_value;
}

pub fn return_struct2(x:i32, y:i32) -> custom_struct {
	let return_value:custom_struct = {x, [y, 3, 4, 5], 'a'};

	ret return_value;
}


pub fn main() -> i32 {
	//Should return 3 + 5 = 8
	OUNIT:[console = 8]
	ret @return_struct(1, 2):y[3] + @return_struct2(1, 2):y[3];
}
