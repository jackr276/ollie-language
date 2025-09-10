/**
* Author: Jack Robbins
* 
* Test handling of postinc/postdec
*/

pub fn struct_testing(arg:i32, argv:char**) -> i32{
	/**
	* Size should be: 1 + 3 pad + 320 + 1 + 3 pad + 4
	*/
	define struct my_struct{
		mut ch:char;
		mut x:i32[80];
		mut lch:char;
		mut y:i32;

	} as my_struct;

	declare mut structure:my_struct;

	structure:ch := 'a';
	structure:x[3] := 3;
	structure:x[5] := 2;
	structure:lch := 'b';
	structure:y := 5;

	//Try to postdec
	structure:ch--;

	//So it isn't optimized away
	ret structure:x[2];
}


pub fn main() -> i32 {
	//The compiler should detect and count the number
	//in the array initializer list.
	let mut arr:i32[] := [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

	arr[3]++;

	ret arr[3];
}
