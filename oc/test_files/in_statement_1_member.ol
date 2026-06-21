/**
* Author: Jack Robbins
* Test an example where a user has written an in statement with only one member. While there is nothing
* wrong with this, we will throw a warning suggesting a rewrite
*/


pub fn in_single_member(x:i32) -> i32 {
	ret x in (1);
}


pub fn main() -> i32 {
	OUNIT: [console = 1]
	ret @in_in_single_member(1);
}
