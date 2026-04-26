/**
* Author: Jack Robbins
* Test our ability to declare more than just one namespace in a composite namespace
* decalaration
*/


namespace composite::nmspace {
	pub fn my_fn() -> i32 {
		ret 5;
	}
}

namespace other::nmspace {
	pub fn my_fn() -> i32 {
		ret 5;
	}
}


//Should return 5 if everything is working
pub fn main() -> i32 {
	ret @composite::nmspace::my_fn() + @other::nmspace::my_fn();
}
