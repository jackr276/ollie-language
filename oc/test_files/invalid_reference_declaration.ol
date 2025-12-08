/**
* Author: Jack Robbins
* Test an invalid attempt to declare a reference. Reference types
* must be declared and initialized using let
*/

pub fn main() -> i32 {
	//Should fail
	declare x:mut i32&;

	ret 0;
}
