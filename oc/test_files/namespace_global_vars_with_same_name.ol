/**
 * Author: Jack Robbins
 * Test the abilities of our name mangler to handle global
 * variables in different namespaces with the same name
 */


namespace space1 {
	let pub x:i32 = 5;
}


namespace space2 {
	let pub x:i32 = 100;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 105]
	ret space1::x + space2::x;
}
