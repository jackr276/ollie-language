/**
 * Author: Jack Robbins
 * Test a case where we perform return region forwarding into a complex
 * (offsetted) region. Because Ollie guarantees execution order from left
 * to right, even though the varible x is modified by the called function,
 * this should still work
 */


define struct my_struct {
	x:i32[5];
	y:i64;
	z:f64;
};



pub fn rrf_ineligible(x:mut i32*, y:i32, z:i64) -> struct my_struct {
	*x = 1;

	ret {[y, y + 1, y + 2, y + 3, y + 4], z, 0};
}


pub fn main() -> i32 {
	declare results:mut struct my_struct[3];
	let x:mut i32 = 0;
	let old_x:i32 = x;

	results[x] = @rrf_ineligible(&x, 5, 8);

	OUNIT: [exit_status = 7]
	ret results[old_x]:x[2];
}


