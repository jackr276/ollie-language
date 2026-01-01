/**
* Author: Jack Robbins
* Test the compiler's ability to handle both f32 and f64 global
* variables only
*/

let x:mut f32 = 0.2222;
let y:mut f64 = 0.2222D;

let float_arr:mut f32[] = [0.222, 3., .3, 17.23];

let double_arr:mut f64[] = [0.222, 3., .3, 17.32];

//Dummy
pub fn main() -> i32 {
	ret 0;
}
