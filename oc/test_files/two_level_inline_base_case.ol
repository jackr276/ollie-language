/**
 * Author: Jack Robbins
 * Test the most basic case where we have an inlined function that inlines another function
 */

inline fn level2() -> i32 {
	ret 5;
}


inline fn level1() -> i32 {
	ret @level2();
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 5]
	ret @level1();
}
