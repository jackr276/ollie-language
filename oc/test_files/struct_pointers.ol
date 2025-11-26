/**
* Author: Jack Robbins
* This program is made for the purposes of testing struct pointers
*/

/**
* Size should be: 1 + 3 pad + 12 + 1 + 3 pad = 20
*/
define struct my_struct{
	ch:mut char;
	y:mut i32[3];
	lch:mut char;
} as custom_struct;


/**
* A function that will mutate a structure in its entirety
*/
pub fn mutate_structure_pointer(struct_pointer:mut custom_struct*) -> i32 {
	struct_pointer=>y[2] = 32;
	struct_pointer=>lch = 'a';

	ret 0;
}


pub fn main(arg:i32, argv:char**) -> i32{
	//Declare and initialize a struct
	let a:mut custom_struct = {'a', [2,3,4], 'b'};

	//Call the mutator
	@mutate_structure_pointer(&a);

	//Grab the value from the internal array argument
	ret a:y[1];
}
