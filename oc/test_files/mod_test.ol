pub fn main() -> i32{
	let x:mut i32 = 3;
	let y:mut i32 = x - 1;

	let z:mut i32 = y % x;

	ret z;
}
