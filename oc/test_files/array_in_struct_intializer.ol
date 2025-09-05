/**
* Author: Jack Robbins
* This program is made for the purposes of testing arrays & structs
*/


/**
* Struct should be:
*  ch: starts at 0, 1 byte ends at 1
*  y: starts at 4, 12 bytes ends at 16
*  lch: starts at 16, 1 byte ends at 17
*  Final alignment, multiple of 4, so 20
*  
*/
define struct my_struct{
	mut ch:char;
	mut y:i32[3];
	mut lch:char;
} as custom_struct;


pub fn main(arg:i32, argv:char**) -> i32{
	//Declare and initialize a struct
	let mut a:custom_struct := {'a', [2,3,4], 'b'};

	//Grab the value from the internal array argument
	ret a:y[arg];
}
