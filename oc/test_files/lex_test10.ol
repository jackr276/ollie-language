define construct my_struct{
	u32 i;
	u32 j;
	u32 prog;
};

alias u8* as int_ptr;

fn unused() -> u16{
	let char* str_literal := "I do nothing";

	ret 2;
}


fn my_func(u32 error_code) -> u32{
	let u32 l := 0;
	let u32 a := 1;
	let u32 k := 0;
	declare construct my_struct*[500] struct_arr;

	let u32 struct_access := struct_arr[3]=>i;
	
	declare f64 i_do_not_exist;

	@main(i, j, i_do_not_exist); 
}
