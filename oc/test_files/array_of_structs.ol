/**
* This program is made for the purposes of testing arrays
*/

pub fn main(arg:i32, argv:char**) -> i32{
	define construct my_struct{
		mut ch:char;
		mut lch:char;
		mut y:i32;

	} as my_struct;

	//Declare an array of such items
	declare mut structure_arr:my_struct[32];

	structure_arr[2]:y := 23;
	structure_arr[2]:ch := 'a';

	ret structure_arr[3]:y;
}
