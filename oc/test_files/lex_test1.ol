/**
* Sample program
*/

declare fn never_defined(mut i32) -> u32;

declare j:u8;

alias u8[100] as int_arr;

fn test_func(my_float:u32, ptr:void*) -> void{
	let mut i:u32 := 0x01;
	//Cast ptr to an int

	declare mut my_arr:i32[100];

	my_arr[3] := 2;

	let z:i32 := 3;
	let x:i32 := -z;

	//Example call
	i := @never_defined(17);

	ret;
}

fn never_defined(mut x:i32) -> u32{
	ret x;
}

pub fn main() -> i32 {
	let mut i:u32 := 1;
	let j:u32 := 1;
	let j_ptr:u32* := &j;
	let i_ptr:u32* := &i;

	//Example call
	i := @never_defined(2);

	ret *i_ptr + *j_ptr;
}
