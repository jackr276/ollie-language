/**
* Author: Jack Robbins
* Testing type coercion rules
*/

//Declare the union type
define union my_union {
	x:i32[5];
	y:i16;
	ch:char;
} as custom_union;

//Define a struct that has this union in it
define struct my_struct {
	tester:mut custom_union;
	x:mut i64;
	a:mut char;
} as custom_struct;

pub fn test_unions() -> i32{
	//Get x as a struct
	declare x:custom_struct;

	//Since this is a mutable char, we can change it
	//even though the top level struct is immutable
	x:tester.ch = 'a';

	x:tester.x[2] = 2;
	x:tester.x[3] = 2;
	x:tester.x[4] = 2;

	x:a = 'a';

	ret x:a + x:tester.x[3];
}

pub fn test_array_in_union() -> i32{
	declare my_union:mut custom_union;
	
	//Store x
	my_union.x[2] = 32;
	
	//Read as char
	ret my_union.ch + my_union.x[3];
}

pub fn main(argc:i32, argv:char**) -> i32 {
	define struct s {
		x:i32;
		y:char;
	} as my_struct;

	let custom:mut my_struct = {5, 'a'};

	//Should be casted to an int
	ret custom:y + custom:x;
}
