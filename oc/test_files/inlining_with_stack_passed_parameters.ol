/**
 * Author: Jack Robbins
 * Test an inlined function that has more than 6 GP passed parameters and therefore
 * will pass some of them via the stack
 */


inline fn pass_by_stack(x:i32, y:i32, z:i32, a:i32, b:i32, c:i32, d:i32, e:i32) -> i32 {
	ret x + y + z + a + b + c + d + e;
}



pub fn main() -> i32 {
	OUNIT: [exit_status = 36]
	ret @pass_by_stack(1, 2, 3, 4, 5, 6, 7, 8);
}
