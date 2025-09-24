/**
* Author: Jack Robbins
* This file will test an array within a union type in ollie
*/

//Declare the union type
define union my_union {
	mut x:i32[5];
	mut y:i16;
	mut ch:char;
} as custom_union;

//Define a struct that has this union in it
define struct my_struct {
	mut tester:custom_union;
	mut x:i64;
	mut a:char;
} as custom_struct;

pub fn main() -> i32{
	//Get x as a struct
	declare mut x:custom_struct;

	x:tester.ch = 'a';

	x:tester.x[2] = 2;
	x:tester.x[3] = 2;
	x:tester.x[4] = 2;

	x:a = 'a';

	ret x:a + x:tester.x[3];
}
