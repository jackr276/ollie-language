pub fn main(argc:i32, argv:char**) -> i32 {
	let x:mut u32 = 32;
	
	let y:mut u32* = &x;

	*y = 32 + *y;

	ret *y;
}
