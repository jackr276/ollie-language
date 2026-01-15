/**
* Author: Jack Robbins
* Test Ollie's handling of post inc/dec on floats, which is different
* than other operations
*/


fn preinc_floats(x:mut f32, y:mut f32) -> f32 {
	x++;
	y--;
	ret x + y;
}

//Dummy
pub fn main() -> i32 {
	ret 0;
}
