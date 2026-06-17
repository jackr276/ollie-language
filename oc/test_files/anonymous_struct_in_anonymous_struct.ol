/**
* Author: Jack Robbins
* Test our ability to handle anonymous struct declarations inside of an existing
* struct declaration *and* nested inside of another anonymous struct
*/


define struct a {
	x:mut i32;
	y:mut i16;
	/**
	* We can skip the define keyword here. Also, b
	* does not need a name for the type as it's only
	* ever used here
	*/
	b: define mut struct {
		x:mut i32[5];
		y:mut f32;
		z:define mut struct {
			x:i32;
			y:i32;
		};
	};
};


pub fn main() -> i32 {
	let tester:struct a = {1, 2, {[1, 2, 3, 4, 5], 5.5, {11, 12}}};

	//Verify that we can distinguish the three x's
	OUNIT: [console = 14]
	ret tester:x + tester:b:x[1] + tester:b:z:x;
}
