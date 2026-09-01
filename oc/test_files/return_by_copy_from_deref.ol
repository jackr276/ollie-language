/**
 * Author: Jack Robbins
 * Test a return by copy that has a dereferecned variable as it's source
 */


define struct my_struct {
	x:mut i32;
	y:mut i32[4];
	z:mut char;
} as custom_struct;


pub fn populate_struct(x:mut struct my_struct*) -> void {
	x=>x = 5;
	x=>y[0] = 1;
	x=>y[1] = 2; 
	x=>y[2] = 3;
	x=>y[3] = 4;
	x=>z = 'a';
}


pub fn return_from_deref() -> struct my_struct {
	//Declare it first
	declare original:mut struct my_struct;

	//Get a pointer to it
	let ptr:mut struct my_struct* = &original;

	//Populate it
	@populate_struct(ptr);

	//Return with a deref as the source
	ret *ptr;
}


pub fn main() -> i32 {
	OUNIT: [exit_status = 4]
	ret @return_from_deref():y[3];
}
