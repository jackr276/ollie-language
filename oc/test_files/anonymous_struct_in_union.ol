/**
* Author: Jack Robbins
* Test our ability to handle anonymous struct declarations inside of an existing
* union declaration
*/


define union a {
	x:define mut struct {
		a:mut i32;
		b:mut i32;
		c:i64;
	};

	y:define mut struct {
		a:mut f32;
		b:mut f32;
		c:f64;
	};
};


pub fn main() -> i32 {
	declare tester:mut union a;

	tester.x:a = 5;
	tester.x:b = 7.0;

	//Should give us back 7
	OUNIT: [exit_status = 7]
	ret tester.x:b;
}
