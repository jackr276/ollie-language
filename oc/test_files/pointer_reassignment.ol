/**
* Author: Jack Robbins
* This program is made for the purposes of testing struct pointers
*/

/**
* Size should be 1 + 3 pad + 12 + 1 + 3 pad = 20
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
	//Reassign the pointer
	let mut ptr:custom_struct* := struct_pointer;

	ptr::y[2] := 32;
	ptr::lch := 'a';

	ret 0;
}


/**
* A function that will mutate a structure in its entirety
*/
pub fn mutate_structure_double_pointer(mut struct_pointer:custom_struct**) -> i32 {
	//Reassign the pointer
	let mut ptr:custom_struct* := *struct_pointer;

	ptr::y[2] := 32;
	ptr::lch := 'a';

	ret 0;
}


/**
* A function that will mutate a structure in its entirety
*/
pub fn reassign_pointer() -> i32 {
	//Declare and initialize a struct
	let mut a:i32[] := [2, 3, 4, 5, 17];

	//Some reassignment
	a[2] := 2;

	//Pointer to a
	let mut c:i32* := a;

	//Some reassignment
	c[1] := 2;
	c[3] := 4;

	//This will mark it
	ret a[3]; // REALLY TRICKY HERE - C is still affecting this
}


pub fn main(arg:i32, argv:char**) -> i32{
	//Declare and initialize a struct
	let mut a:custom_struct := {'a', [2,3,4], 'b'};

	let mut b:custom_struct* := &a;

	//Call the mutator
	@mutate_structure_pointer(b);
	//Call the double pointer mutator
	@mutate_structure_double_pointer(&b);

	//Grab the value from the internal array argument
	ret a:y[1];
}
