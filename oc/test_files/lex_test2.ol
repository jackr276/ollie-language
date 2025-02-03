func:static main() -> i32 {
	let u32 i := 0;
	//Useless, just testing
	defer i++;

	declare char* str;
	declare void* void_ptr;
	let u32 bad := *void_ptr;

	let char* my_str := "I am a string";
	
	ret 3;
}
