/**
* Author: Jack Robbins
* Testing nested function call handling
*/


fn j(x:i32) -> i32 {
	ret x << 8;
}

fn i(x:i32, j:i32) -> i32 {
	ret x >> j;
}

fn k(x:i32) -> i32 {
	ret x / 8;
}

pub fn main() -> i32 {
	ret @i(@j(@k(32222)), @j(3)) + 3;
}
