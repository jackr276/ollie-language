/**
* Author: Jack Robbins
* Test our ability to handle anonymous union declarations inside of an existing
* struct declaration
*/


define struct a {
	x:mut i32;
	y:mut i16;
	/**
	* We can skip the define keyword here. Also, b
	* does not need a name for the type as it's only
	* ever used here
	*/
	b: define mut union {
		x:mut i32;
		y:mut f32;
	   };
};


pub fn main() -> i32 {
	declare tester:mut struct a;

	//Populate the tester
	tester:x = 5;
	tester:y = 4;
	tester:b.x = 11;

	//Verify that we can distinguish the two x's
	OUNIT: [exit_status = 16]
	ret tester:x + tester:b.x;
}
