/**
* Author: Jack Robbins
* Test an invalid attempt to initialize a struct using the array([<list>]) syntax
*/


/**
* Should be 24 in size(4 + 4 pad + 8 + 1 + 3 pad + 4 pad(multiple of 8) = 24)
*/
define struct custom {
		x:mut i32;
		a:mut i64;
		y:mut char;
} as my_struct;


pub fn main() -> i32 {
	//Will fail
	let x:mut my_struct = [3, 5l, 'a'];

	ret x.x;
}
