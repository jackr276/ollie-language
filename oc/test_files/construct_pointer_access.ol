/**
* This program is made for the purposes of testing case statements
*/
define construct my_struct{
		mut ch:char;
		mut x:i64;
		mut lch:char;
		mut y:i32;

	} as my_struct;


fn construct_pointer_arrays(arg:i32) -> i64 {
	//An array of structure pointers
	declare mut arr:my_struct*[232];

	arr[2]=>x := 32;

}


fn not_main(arg:i32, argv:char**) -> i64 {
	//Stack allocate a structure
	declare mut structure:my_struct;
	//Take it's address
	let mut struct_ptr:my_struct* := &structure;

	struct_ptr=>ch := 'a';
	struct_ptr=>x := 3;
	struct_ptr=>lch := 'b';
	struct_ptr=>y := 5;

	if(arg == 0) {
		struct_ptr=>x := 2;
		struct_ptr=>y := 1;
		struct_ptr=>lch := 'c';
	} else{
		struct_ptr=>y := 5;
	}

	//Useless assign
	let mut z:i64 := struct_ptr=>x;

	//structure:x := 7;
	let x:i64 := struct_ptr=>x;

	//So it isn't optimized away
	ret x;
}


pub fn main(arg:i32, argv:char**) -> i32{
	@not_main(arg, argv);
	ret arg;
}
