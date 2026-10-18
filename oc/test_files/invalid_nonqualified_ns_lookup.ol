/**
 * Author: Jack Robbins
 * Test a case where we are trying to do a non-qualified namespace lookup for a value
 * that is not in the global scope
 */

namespace my_ns1 {
	namespace my_ns1_child {
		pub fn sample() -> i32 {
			ret 0;
		}
	}

	namespace my_ns2 {
		namespace my_ns2_child {
			//INVALID - we can't see into here
			pub fn dummy() -> i32 {
				ret @my_ns1_child::sample();
			}
		}
	}
}


pub fn main() -> i32 {
	OUNIT: [fail_to_compile]
	ret 0;
}

