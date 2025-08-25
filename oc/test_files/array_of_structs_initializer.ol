/**
* Author: Jack Robbins
* This program is made for the purposes of testing arrays
*/

define struct my_struct{
	mut ch:char;
	mut lch:char;
	mut y:i32;
} as my_struct;


pub fn main(arg:i32, argv:char**) -> i32{
	//Declare an array of such items

	//Declare the struct array
	let mut structure_arr:my_struct[] := [{'a', 'b', 23}, {'b', 'a', 22}, {'a', 'b', 222}];

	//grab the structure array at the argument at y
	ret structure_arr[arg]:y;
}
