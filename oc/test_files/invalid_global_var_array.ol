/**
* Author: Jack Robbins
* Test an invalid attempt to initialize a global array
* using other global variables. This does not work. It is 
* only allowed to use constants
*/

//Initialize some global vars here
let y:i32 = 2;
let z:i32 = 2;
let a:i32 = 2;

let x:i32[] = [a, 2, a, 4, z, 6, 7];

//Dummy just for testing
pub fn main() -> i32 {
	ret	x[3];
}
