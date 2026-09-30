/**
 * Author: Jack Robbins
 * Test our ability to copy into a static struct variable
 */

define struct my_struct {
	x:i32;
	arr:i32[10];
	y:f32;
} as custom_struct;



pub fn reentrant_struct_handler(ptr:struct my_struct*) -> i32 {
	//How many times we've been called
	let static call_count:mut i32 = 0;
	//Static struct var
	declare static static_struct:mut struct my_struct;

	//Result that we return in the end
	declare result:i32;

	if(call_count == 0){
		static_struct = *ptr;
		result = -1;
	} else {
		result = static_struct:arr[7];
	}

	call_count++;
	ret result;
}



pub fn main() -> i32 {
	let dummy:struct my_struct = {5, [20, 21, 22, 23, 24, 25, 26, 27, 28, 29], 'a'};

	//First call populates
	@reentrant_struct_handler(&dummy);
	
	OUNIT: [exit_status = 27]
	ret @reentrant_struct_handler(&dummy);
}
