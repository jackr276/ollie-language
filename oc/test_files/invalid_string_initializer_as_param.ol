/**
 * Author: Jack Robbins
 * Test the invalid use of a string initializer as a parameter. This is invalid because array types(char[] included)
 * are always passed by reference and never by copy. Because of this, the string initializer would break our model
 * so we disallow it
 */


pub fn use_array(x:char[5]) -> i32 {
	ret x[1] + x[2];
}


pub fn main() -> i32 {
	OUNIT: [fail_to_compile]
	ret @use_array("bonk");
}
