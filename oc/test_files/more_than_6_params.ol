/**
* Author: Jack Robbins
* Test the very basic case where a function has more than 6 parameters
*/


pub fn more_than_6(x:i32, y:i32, z:i32, aa:i32, bb:i32, cc:i32, dd:i32) -> i32 {
	ret x + y + z * aa - bb + cc - dd;
}



pub fn main() -> i32 {
	ret @more_than_6(1, 2, 3, 4, 5, 6, 7);
}
