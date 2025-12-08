/**
* Author: Jack Robbins
* Test an invalid attempt to use a reference in the global scope. References
* may only be used within functions
*/


let y:mut i32 = 3;
//INVALID - cannot use references globally
let x:mut i32& = y;

//Dummy
pub fn main() -> i32 {
	ret 0;
}
