/**
* Sample program
*/

//All preprocessor commands must be handled here
__BEGIN__


__END__


fn never_defined(example:f64) -> u32;

declare j:u8;

alias u8[100] as int_arr;

fn test_func(my_float:u32, ptr:void*) -> void{
	let mut i:u32 := 0x01;
	//Cast ptr to an int
	i := *(<u32*>ptr);

	declare mut my_arr:i32[100];

	my_arr[3] := 2.23;

	let my:void := ptr;

	//Example call
	i := @never_defined(3.2222);

	ret;
}

fn never_defined(example:f64) -> i32{
	ret 2;
}

fn:static main() -> i32 {
	let mut i:u32 := 1;
	let j:u32 := 1;
	let j_ptr:u32* := &j;
	let i_ptr:u32* := &i;

	//Example call
	i := @never_defined(2.32);

	ret *i_ptr + *j_ptr;
}
