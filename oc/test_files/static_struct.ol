/**
* Author: Jack Robbins
* Test our handling of static structs
*/


define struct my_struct {
	x:mut i32;
	y:mut i32[5];
	z:mut f32;
};



pub fn static_struct(i:bool) -> i32 {
	let static tester:struct my_struct = {1, [1, 2, 3, 4, 5], 5.5};

	if(i){
		ret tester:x;
	} else {
		ret tester:y[2]; 
	}
}


pub fn main() -> i32 {
	OUNIT:[console = 3]
	ret @static_struct(false);
}
