/**
 * Author: Jack Robbins
 * Test our ability to still indirectly call an inlined function
 */


inline fn inline_add(x:i32, y:i32) -> i32 {
	ret x + y;
}


pub fn indirectly_call(x:i32, y:i32) -> i32 {
	let fn_ptr:fn(i32, i32) -> i32 = inline_add;

	ret @fn_ptr(x, y);
}


pub fn main() -> i32 {
	let x:i32 = 5;
	let y:i32 = 6;
	let z:i32 = 7;
	let a:i32 = 8;
	
	//Should in the end return 5 + 6 + 7 + 8 = 26
	OUNIT: [exit_status = 26]
	ret @inline_add(x, y) + @indirectly_call(z, a);
}
