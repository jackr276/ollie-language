/**
* Author: Jack Robbins
* Test what happens if we take the address of an array twice
*/

//Ollie macro definition
#macro NULL 0l #endmacro

define struct my_struct {
	next:mut struct my_struct*;
	a:mut i32;
	b:mut char;
} as custom_struct;

pub fn mutate_struct(a:mut custom_struct**) -> i32{
	(*a)=>a = 3;
	(*a)=>b = 3;

	ret (*a)=>b;
}


pub fn main() -> i32 {
	//Initialize the struct
	let str:mut custom_struct = {<u64>NULL, 3, 3};

	//Grab a reference
	let x:mut custom_struct* = &str;

	//Insert this with the struct's address
	@mutate_struct(&x);

	//Insert this with the struct's address
	@mutate_struct(&x);

	ret str:a;
}
