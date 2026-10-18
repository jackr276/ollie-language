/**
 * Author: Jack Robbins
 * Test our ability to call a function in a child namespace
 */


namespace my_ns1 {
	//predeclare
	declare fn helper(mut i32*) -> void;

	namespace my_ns2 {
		pub fn child() -> i32 {
			let x:mut i32 = 4;
			@helper(&x);
			ret x;
		}
	}

	fn helper(x:mut i32*) -> void {
		*x += 1;
	}


	pub fn call_child() -> i32 {
		ret @my_ns2::child();
	}
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 5]
	ret @my_ns1::call_child();
}
