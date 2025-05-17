fn main(argc:i32, argv:char**) -> i32 {
	let mut x:u32 := 32;
	
	let mut y:u32* := &x;

	*y := 32 + *y;

	ret *y;
}
