/**
* Special file for testing the instruction selector
*/

let mut a:i32 := 3;


fn main(arg:i32, argv:char**) -> i32 {
	let mut x:i32 := -1;

	let mut y:i32 := -3;
	x := y;

	x := x + y * 8;
	a := x - 0;

	a := x * 0;

	a := x * -128;

	a := a + y;
	x := x / 8;
	x := x + 8;

	ret x + y + a;
}
