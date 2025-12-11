/**
* Author: Jack Robbins
* Attempt to initialize a global var to another global var. This is invalid.
* Global variables may only be initialized to constants
*/

let x:i32 = 3;
//INVALID
let y:i32 = x;

pub fn main() -> {
	ret x + y;
}
