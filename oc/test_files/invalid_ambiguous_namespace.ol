/**
 * Author: Jack Robbins
 * Test a case where we have an ambiguous namespace
 */


namespace my_ns1 {
	namespace ambiguous {
		pub fn my_fn() -> i32 {
			ret 5;
		}
	}
}

namespace ambiguous {
	pub fn my_fn() -> i32 {
		ret 2;
	}
}


pub fn main() -> i32 {
	//Which namespace do we mean? Since this is ambiguous we'll fail
	OUNIT: [fail_to_compile]
	ret @ambiguous::my_fn();
}


