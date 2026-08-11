/**
 * Author: Jack Robbins
 * Test the use of an undefined function
 */


namespace ns {
	declare pub fn my_fn(i32) -> i32;
}



pub fn main() -> i32 {
	OUNIT:[fail_to_compile]
	ret @ns::my_fn(5);
}
