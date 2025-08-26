/**
* Author: Jack Robbins
* This program is made for the purposes of testing arrays & structs
*/

define struct my_struct{
	mut y:i32[3];
	mut ch:char;
	mut lch:char;
} as custom_struct;


pub fn main(arg:i32, argv:char**) -> i32{
	//Declare and initialize a struct
	let mut a:custom_struct := {[2,3,4], 'a', 'b'};

	//Grab the value from the internal array argument
	ret a:y[arg];
}
