/**
 * Author: Jack Robbins
 * Test an edge case where we have multiple inlined calls back to back in the same block
 */


inline fn add(x:mut i32*, y:i32) -> void {
	*x += y;
}


inline fn sub(x:mut i32*, y:i32) -> void {
	*x -= y;
}


fn add_or_subtract(x:mut i32, use_add:bool) -> i32 {
	if(use_add){
		@add(&x, 5);
		@add(&x, 6);
	} else {
		@sub(&x, 5);
		@sub(&x, 6);
	}

	ret x;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 16]
	ret @add_or_subtract(5, true);
}
