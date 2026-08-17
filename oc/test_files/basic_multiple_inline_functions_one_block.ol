/**
 * Author: Jack Robbins
 * Test a very very basic case where we have multiple inlined functions ostensibly in the same
 * exact block
 */


pub inline fn my_fn1() -> i32 {
	ret 5;
}


pub inline fn my_fn2() -> i32 {
	ret 11;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 16]
	ret @my_fn1() + @my_fn2();
}
