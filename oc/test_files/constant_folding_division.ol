pub fn main() -> i32{
	let mut x:i32 := 3;
	let mut y:i32 := x - 1;

	//This should optimize into a left shift
	let mut z:i32 := y / 4;

	ret z + x;
}
