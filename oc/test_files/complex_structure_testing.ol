/**
* This program is made for the purposes of testing arrays
*/

pub fn main(arg:i32, argv:char**) -> i32{
	/**
	* Size should be: 1 + 3 pad + 320 + 1 + 3 pad + 4
	*/
	define struct my_struct{
		ch:mut char;
		x:mut i32[80];
		lch:mut char;
		y:mut i32;

	} as my_struct;

	declare structure:mut my_struct;

	structure:ch = 'a';
	structure:x[3] = 3;
	structure:x[5] = 2;
	structure:lch = 'b';
	structure:y = 5;

	//So it isn't optimized away
	ret structure:x[2];
}
