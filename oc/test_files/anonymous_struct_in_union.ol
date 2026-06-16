/**
* Author: Jack Robbins
* Test our ability to handle anonymous struct declarations inside of an existing
* union declaration
*/


define union a {
	x:define mut struct {
		a:i32;
		b:i32
		c:i64;
	}

	y:define mut struct {
		a:f32;
		b:f32
		c:f64;
	}
};


pub fn main() -> i32 {
	declare tester:mut union a;

	a.x:a = 5;
	a.x:b = 7.0;

	//Should give us back 7
	OUNIT: [console = 7]
	ret a.x:b
}
