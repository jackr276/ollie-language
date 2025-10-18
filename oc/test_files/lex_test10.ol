define struct my_struct{
	i:u32;
	j:u32;
	prog:u32;
};

alias u8* as int_ptr;

fn unused() -> u32{
	let str_literal:char* = "I do nothing";

	ret 2;
}

fn empty_func() -> void {

}


fn my_func(error_code:u32) -> u32{
	let l:u32 = 0;
	let a:u32 = 1;
	let k:u32 = 0;
	declare struct_arr:struct my_struct[300];

	let struct_access:u32 = struct_arr[3]:i;
	
	declare i_do_not_exist:f64;
}
