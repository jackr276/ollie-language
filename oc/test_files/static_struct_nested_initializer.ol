/**
* Author: Jack Robbins
* Test our handling of static struct initialization with nested structs
*/


define struct my_struct {
	c:mut i8;
	//Needed padding
	y:mut i32[5];
	a:define mut struct {
		x:mut i32;
		y:mut i8;
	  };
	z:mut f32;
	aa:mut i64;
};



pub fn static_struct(i:bool) -> i32 {
	let static tester:struct my_struct = {1, [1, 2, 3, 4, 5], {7, 8}, 5.5, 15};

	if(i){
		ret tester:c + tester:a:x;
	} else {
		ret tester:y[2] + tester:a:y; 
	}
}


pub fn main() -> i32 {
	OUNIT:[exit_status = 11]
	ret @static_struct(false);
}
