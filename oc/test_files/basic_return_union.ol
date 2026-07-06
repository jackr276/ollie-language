/**
* Author: Jack Robbins
* Test the most basic case where we are returning a union
*/


//Very basic struct that we are using in our union
define struct my_struct {
	x:i32;
	y:i64;
	z:char;
} as custom_struct;


//Basic union with struct & int that we are returning
define union my_union {
	struct_value:mut custom_struct;
	int_value:mut i32;
} as custom_union;



pub fn return_union() -> custom_union {
	//Custom union to return
	declare ret_val:mut custom_union;

	//Struct to stuff it with
	let inner_struct:custom_struct = {3, 4, 'a'};

	//Copy assignment
	ret_val.struct_value = inner_struct;

	
	ret ret_val;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 'a']
	ret @return_union().struct_value:z;
}
