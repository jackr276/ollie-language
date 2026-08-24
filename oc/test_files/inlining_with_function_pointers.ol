/**
 * Author: Jack Robbins
 * Test our ability to inline when we have function pointer variables
 */

fn subtract(x:i32, y:i32) -> i32 {
	ret x - y;
}

fn add(x:i32, y:i32) -> i32 {
	ret x + y;
}


inline fn make_determination(should_add:bool) -> (fn(i32, i32) -> i32) {
	if(should_add){
		ret add;
	} else {
		ret subtract;
	}
}


pub fn main() -> i32 {
	let x:i32 = 5;
	let y:i32 = 6;
	let decider:bool = true;

	let determined_fn:fn(i32, i32) -> i32 = @make_determination(decider);

	OUNIT: [exit_status = 11]
	ret @determined_fn(x, y);
}
