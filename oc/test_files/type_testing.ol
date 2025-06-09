fn main(mut argc:u32) -> i32{
	let mut a:void* := &argc;
	let mut i:u64 := typesize(void*) + sizeof(a);

	// a := a + 1 TODO BROKEN

	ret argc;
}
