/**
 * Author: Jack Robbins
 * Test our ability to perform a deferred function call
 */


pub fn test_fn(x:mut i32*) -> void {
	*x *= 2;
}


pub fn defer_call(x:i32) -> i32 {
	let result:mut i32 = 0;

	defer {
		@test_fn(&result);
	}

	for(let i:mut i32 = 0; i < x; i++){
		result++;
	}

	ret result;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 10]
	ret @defer_call(5);
}
