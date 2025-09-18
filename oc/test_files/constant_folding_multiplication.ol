pub fn main() -> i32{
	let mut x:u32 := 3;
	let mut y:u32 := x - 1;

	//This should optimize into a right shift
	let mut z:u32 := y * 4;

	ret z + x;
}
