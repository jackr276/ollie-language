

pub fn main() -> i32 {
	let i:u32 = 0;
	//Useless, just testing
	defer i++;

	declare str:char*;
	declare void_ptr:void*;
	let bad:u32 = *void_ptr;

	let my_str:char* = "I am a string";
	
	ret 3;
}
