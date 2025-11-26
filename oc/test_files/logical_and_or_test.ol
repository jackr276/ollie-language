/**
* A simple testing file to test logical and, or and not
*/



pub fn main(arc:i32, argv:char**) -> i32 {
	let x:mut i32 = 73;
	let y:mut i32 = 88;

	let z:i32 = x || y;
	let a:i32 = z && y;
	let c:i32 = !a;
	
	ret x + a + c;
}
