fn main() -> i32{
	let mut x:i32 := 3;
	let mut y:i32 := x - 1;

	let mut z:i32 := y / x;
	let mut k:i32 := y * sizeof(x);

	ret z + k * typesize(f64);
}
