/**
 * Author: Jack Robbins
 * Test an attempt to grab a non-public global variable
 * from a namespace
 */


namespace my_space {
	//Not public
	let x:i32 = 5;
}


pub fn main() -> i32 {
	//INVALID - not visible it's non-public
	OUNIT: [fail_to_compile]
	ret my_space::x;
}
