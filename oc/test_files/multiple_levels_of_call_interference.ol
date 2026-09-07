/**
 * Author: Jack Robbins
 * Test our ability to do saving with multiple levels of caller interference
 */


inline fn inline_add(x:i32, y:i32) -> i32 {
	ret x + y;
}


//Should clobber %rdx
fn dummy_fn_with_many_params(x:i32, y:i32, z:i32, a:i32, b:i32) -> i32 {
	ret x + y + z + a + b;
}


pub fn call_dummy(x:i32) -> i32 {
	ret @dummy_fn_with_many_params(x, 1, 2, 3, 4);
}


pub fn indirectly_call(x:i32, y:i32) -> i32 {
	let fn_ptr:fn(i32, i32) -> i32 = inline_add;

	//Just to see the interference
	@call_dummy(x);

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
