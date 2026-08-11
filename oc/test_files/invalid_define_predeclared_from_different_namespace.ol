/**
 * Author: Jack Robbins
 * Test an invalid case where we try to define a predeclared function in a different
 * namespace. You can only define predeclared functions in the same namespace
 */


declare fn my_fn(i32) -> i32;

namespace my_space {
	pub fn my_fn(x:i32) -> i32 {
		ret x << 2;
	}
}


pub fn main() -> i32 {
	OUNIT: [fail_to_compile]
	ret @my_fn(5);
}
