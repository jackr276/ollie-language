/**
 * Author: Jack Robbins
 * Test a basic case where we're inlining a function that calls other functions
 * indirectly to test our ability to indirectly inline functions
 */


fn add_ints(a:i32, b:i32, c:i32, d:i32, e:i32, f:i32) -> i32 {
	ret a + b + c + d + e + f;
}


fn add_floats(a:f32, b:f32, c:f32, d:f32, e:f32, f:f32) -> f32 {
	ret a + b + c + d + e + f;
}


inline fn make_add_decision(use_int:bool) -> i32 {
	let iadd:fn(i32, i32, i32, i32, i32, i32) -> i32 = add_ints;
	let fadd:fn(f32, f32, f32, f32, f32, f32) -> f32 = add_floats;

	if(use_int) {
		ret @iadd(1, 2, 3, 4, 5, 6);
	} else {
		ret @fadd(1, 2, 3, 4, 5, 6);
	}
}


pub fn main() -> i32 {
	//Should return 42 in the end
	OUNIT: [exit_status = 42]
	ret @make_add_decision(true) + @make_add_decision(false);
}
