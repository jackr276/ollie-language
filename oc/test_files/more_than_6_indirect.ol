/**
* Author: Jack Robbins
* Test the ability to pass more than 6 parameters with an indirect function call
*/


pub fn more_than_6_1(x:i32, y:i32, z:i32, aa:i32, bb:i32, cc:i32, dd:i32, ee:i32) -> i32 {
	ret x + y + z * aa - bb + cc - dd + ee;
}


pub fn more_than_6_2(x:i32, y:i32, z:i32, aa:i32, bb:i32, cc:i32, dd:i32, ee:i32) -> i32 {
	ret x + y + z * aa + bb + cc + dd - ee;
}


pub fn more_than_6_3(x:i32, y:i32, z:i32, aa:i32, bb:i32, cc:i32, dd:i32, ee:i32) -> i32 {
	ret x - y - z * aa + bb + cc + dd * ee;
}


pub fn main() -> i32 {
	let func:fn(i32, i32, i32, i32, i32, i32, i32, i32) -> i32 = more_than_6_2; 

	ret @func(1, 2, 3, 4, 5, 6, 7, 8);
}
