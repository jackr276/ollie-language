/**
* Author: Jack Robbins
* This program is made for the purposes of testing struct pointers
*/

/**
* Size should be: 1 + 3 pad + 12 + 1 + 3 pad = 20
*/
define struct my_struct{
	mut ch:char;
	mut y:i32[3];
	mut lch:char;
} as custom_struct;


/**
* A function that will mutate a structure in its entirety
*/
pub fn mutate_structure_pointer(mut struct_pointer:custom_struct*) -> i32 {
	(*struct_pointer):y[2] = 32;
	(*struct_pointer):lch = 'a';

	ret (*struct_pointer):lch;
}


pub fn main(arg:i32, argv:char**) -> i32{
	//Declare and initialize a struct
	let mut a:custom_struct = {'a', [2,3,4], 'b'};

	//Call the mutator
	@mutate_structure_pointer(&a);

	//Grab the value from the internal array argument
	ret a:y[1];
}
