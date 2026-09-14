/**
 * Author: Jack Robbins
 * Test an invalid case where we are defining a function twice with the exact same
 * signature. Since we have the exact same signature, this is not an overload and is
 * treated by the compiler as invalid
 */


pub fn add(x:i32, y:i32) -> i32 {
	ret x + y;
}


pub fn add(a:i32, b:i32) -> i32 {
	ret a + b;
}


pub fn main() -> i32 {
	OUNIT: [fail_to_compile]
	ret 0;
}
