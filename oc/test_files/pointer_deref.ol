fn main(argc:i32, argv:char**) -> i32 {
	let x:u32 := 32;
	
	let y:u32* := &x;

	ret *y;
}
