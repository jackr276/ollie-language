/**
* Author: Jack Robbins
* Testing relational operators
*/

fn main() -> i32{
	let mut x:i32 := 3;
	let mut y:i32 := 3;

	let z:i32 := x >= y;

	if(x <= y) then {
		ret 2;
	}

	ret z;
}
