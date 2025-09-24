/**
* Author: Jack Robbins
* Test a bad postfix operator use
*/

//Test preopping
pub fn preop_pointers(mut x:i32*) -> i32 {
	(++x)[2] = 3;

	ret x[3];

}

//Test postopping
pub fn postop_pointers(mut x:i32*) -> i32 {
	(x++)[2] = 3;

	ret x[3];
}


pub fn main(arg:i32, argv:char**) -> i32 {
	ret 0;
}
