/**
 * Author: Jack Robbins
 * Test our use of comparison operators
 */

pub fn compare_g_than(x:i32, y:i32) -> i32 {
	ret x > y;
}


pub fn compare_l_than(x:i32, y:i32) -> i32 {
	ret x < y;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 1]
	ret @compare_g_than(5, 6) + @compare_l_than(5, 6);
}
