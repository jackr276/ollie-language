/**
 * Author: Jack Robbins
 * Just to cover our bases - test what would happen if we use a totally unknown variable
 */

pub fn main() -> i32 {
	//Should fail - x isn't real
	OUNIT: [fail_to_compile]
	ret x;
}
