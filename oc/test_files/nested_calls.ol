/**
* Author: Jack Robbins
* Testing nested function call handling
*/


fn j(x:i32) -> i32 {
	ret x << 8;
}

fn i(x:i32) -> i32 {
	ret x >> 8;
}

pub fn main() -> i32 {
	ret @i(@j(255888)) + 3;
}
