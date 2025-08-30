/**
* Author: Jack Robbins
* This program is made for the purposes of testing struct pointers
*/

define struct my_struct{
	mut ch:char;
	mut y:i32[3];
	mut lch:char;
} as custom_struct;


/**
* A function that will mutate a structure in its entirety
*/
pub fn mutate_structure_pointer(mut a:custom_struct*) -> i32 {
	a::y[2] := 32;
	a::lch := 'a';

	ret 0;
}


pub fn main(arg:i32, argv:char**) -> i32{
	//Declare and initialize a struct
	let mut a:custom_struct := {'a', [2,3,4], 'b'};

	//Call the mutator
	@mutate_structure_pointer(&a);

	//Grab the value from the internal array argument
	ret a:y[1];
}
