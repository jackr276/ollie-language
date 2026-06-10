/**
* Author: Jack Robbins
* Test an edge case where we are copying a returned struct value into a struct param. This particular
* iteration of the test uses an indirect function call
*/


define struct values {
	x:i32;
	y:i64;
	c:char;
} as value_struct;


/**
* Build up and return a struct
*/
fn construct_struct(x:i32, y:i64, c:char) -> value_struct {
	let ret_val:value_struct = {x, y, c};

	ret ret_val;
}


/**
* Simple function that just makes use of a struct param
*/
fn use_struct_param(x:i32, y:value_struct) -> i32 {
	ret x + y:x + y:c;
}


pub fn main() -> i32 {
	let fn_ptr:fn(i32, value_struct) -> i32 = use_struct_param;

	//Should return 5 + 3 + 97('a') = 105
	OUNIT: [console = 105]
	ret @fn_ptr(5, @construct_struct(3, 4, 'a'));
}
