/**
* Author: Jack Robbins
* This program is made for the purposes of testing double pointers
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
* Test reassigning a double pointer
*/
pub fn double_pointer_reassign(mut ptr:i64**) -> i64**{
	**ptr := 32;

	ret ptr;
}

/**
* Test reassigning a double pointer
*/
pub fn single_pointer_reassign(mut ptr:i64*) -> void{
	*ptr := 32;

	ret;
}




/**
* A function that will mutate a structure in its entirety
*/
pub fn mutate_structure_pointer(mut struct_pointer:custom_struct**) -> i32 {
	(*struct_pointer)::y[2] := 32;
	(*struct_pointer)::lch := 'a';

	ret 0;
}


pub fn main(arg:i32, argv:char**) -> i32{
	//Declare and initialize a struct
	let mut a:custom_struct := {'a', [2,3,4], 'b'};
	
	//One more pointer
	let mut b:custom_struct* := &a;

	let mut x:i64 := 32;
	let mut ptr:i64* := &x;

	@double_pointer_reassign(&ptr);

	//Call the mutator
	@mutate_structure_pointer(&b);

	//Grab the value from the internal array argument
	ret a:y[1];
}
