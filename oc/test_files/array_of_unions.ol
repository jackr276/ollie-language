/**
* This program is made for the purposes of testing arrays
*/

pub fn main(arg:i32, argv:char**) -> i32{
	define union my_union{
		ch:char;
		lch:char;
		y:i32;
	} as my_union;

	//Declare an array of such items
	declare union_array:mut my_union[32];

	union_array[2].y = 23;
	union_array[2].ch = 'a';

	ret union_array[3].y;
}
